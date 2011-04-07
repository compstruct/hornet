// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <algorithm>
#include <numeric>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/task/detail/bind_processor.hpp>
#include "sim.hpp"

template<typename T>
class minimum {
public:
    const T &operator()(const T &x, const T &y) {
        return less_equal<T>()(x, y) ? x : y;
    }
};

sim_thread::sim_thread(uint32_t new_thread_index,
                       shared_ptr<sys> new_sys,
                       const vector<tile_id> &tids,
                       const uint64_t new_num_cycles,
                       const uint64_t new_num_packets,
                       const uint64_t new_sync_period,
                       shared_ptr<barrier> new_sync_barrier,
                       bool &new_global_drained,
                       uint64_t &new_global_next_time,
                       uint64_t &new_global_max_sync_count,
                       vector<bool> &new_per_thread_drained,
                       vector<bool> &new_per_thread_time_exceeded,
                       vector<uint64_t> &new_per_thread_packet_count,
                       vector<uint64_t> &new_per_thread_next_time,
                       bool new_enable_fast_forward,
                       int cpu_affinity,
                       shared_ptr<vcd_writer> new_vcd)
    : my_thread_index(new_thread_index), s(new_sys),
      my_tile_ids(tids.begin(), tids.end()),
      max_num_cycles(new_num_cycles), max_num_packets(new_num_packets),
      sync_period(new_sync_period),
      sync_barrier(new_sync_barrier),
      global_drained(new_global_drained),
      global_next_time(new_global_next_time),
      global_max_sync_count(new_global_max_sync_count),
      per_thread_drained(new_per_thread_drained),
      per_thread_time_exceeded(new_per_thread_time_exceeded),
      per_thread_packet_count(new_per_thread_packet_count),
      per_thread_next_time(new_per_thread_next_time),
      enable_fast_forward(new_enable_fast_forward),
      cpu(cpu_affinity), vcd(new_vcd) { }

void sim_thread::operator()() {
    if (my_tile_ids.empty()) return;
    if (cpu >= 0) {
        this_thread::bind_to_processor(cpu);
    }
    sync_barrier->wait();
    uint64_t sync_count = 0; // count only post-negedge syncs
    while (sync_count < global_max_sync_count) {
        if (vcd) vcd->commit();
        for (vector<tile_id>::const_iterator ti = my_tile_ids.begin();
             ti != my_tile_ids.end(); ++ti) {
            s->tick_positive_edge_tile(*ti);
        }
        uint64_t my_packet_count = 0;
        bool my_drained = true;
        uint64_t my_next_time = UINT64_MAX;
        uint64_t min_clock = UINT64_MAX;
        for (vector<tile_id>::const_iterator ti = my_tile_ids.begin();
             ti != my_tile_ids.end(); ++ti) {
            my_packet_count +=
                s->get_statistics_tile(*ti)->get_received_packet_count();
            my_drained = my_drained && s->is_drained_tile(*ti);
            my_next_time = min(my_next_time, s->advance_time_tile(*ti));
            min_clock = min(min_clock, s->get_time_tile(*ti));
        }
        if ((max_num_cycles != 0) && (max_num_cycles <= min_clock)) {
            per_thread_time_exceeded[my_thread_index] = true;
        }
        assert(my_packet_count >= per_thread_packet_count[my_thread_index]);
        per_thread_packet_count[my_thread_index] = my_packet_count;
        per_thread_drained[my_thread_index] = my_drained;
        if (my_drained) {
            assert(my_next_time >= per_thread_next_time[my_thread_index]);
            per_thread_next_time[my_thread_index] = my_next_time;
        }
        if (sync_period == 0) {
            sync_barrier->wait();
        }
        for (vector<tile_id>::const_iterator ti = my_tile_ids.begin();
             ti != my_tile_ids.end(); ++ti) {
            s->tick_negative_edge_tile(*ti);
        }
        if (my_thread_index == 0) { // no sync, undercount at worst
            bool system_drained =
                accumulate(per_thread_drained.begin(),
                           per_thread_drained.end(), true,
                           logical_and<bool>());
            bool system_time_exceeded = 
                accumulate(per_thread_time_exceeded.begin(),
                           per_thread_time_exceeded.end(), true,
                           logical_and<bool>());
            uint64_t system_packet_count =
                accumulate(per_thread_packet_count.begin(),
                           per_thread_packet_count.end(), 0ULL);
            bool system_num_packets_exceeded =
                ((max_num_packets != 0)
                 && (system_packet_count >= max_num_packets));
            uint64_t system_next_time =
                accumulate(per_thread_next_time.begin(),
                           per_thread_next_time.end(), UINT64_MAX,
                           minimum<uint64_t>());
            if ((system_time_exceeded || system_num_packets_exceeded
                 || (system_drained && system_next_time == UINT64_MAX))) {
                global_max_sync_count = sync_count + 1;
            }
            global_drained = system_drained;
            global_next_time = system_next_time;
        }
        if (vcd) vcd->tick();
        if ((sync_period == 0) || ((min_clock % sync_period) == 0)) {
            sync_barrier->wait();
            ++sync_count;
        }
        if (enable_fast_forward && global_drained) {
            uint64_t ff_time =
                global_next_time == UINT64_MAX ?
                max_num_cycles : global_next_time;
            for (vector<tile_id>::const_iterator ti = my_tile_ids.begin();
                 ti != my_tile_ids.end(); ++ti) {
                if (ff_time > s->get_time_tile(*ti)) {
                    s->fast_forward_time_tile(*ti, ff_time);
                }
            }
            if (vcd && ff_time > vcd->get_time()) {
                vcd->fast_forward_time(ff_time);
            }
        }
    }
    if (vcd) vcd->commit();
}

sim::sim(shared_ptr<sys> s,
         const uint64_t num_cycles, const uint64_t num_packets,
         const uint64_t sync_period, const uint32_t concurrency,
         bool enable_fast_forward,
         tile_mapping_t tile_mapping,
         const vector<unsigned> &cpu_affinities,
         shared_ptr<vcd_writer> vcd, logger &new_log,
         shared_ptr<random_gen> new_rng)
    : global_drained(false), global_next_time(0),
      global_max_sync_count(UINT64_MAX), log(new_log), rng(new_rng) {
    uint32_t hw_concurrency = thread::hardware_concurrency();
    if (hw_concurrency == 0) hw_concurrency = 1;
    uint32_t num_threads = concurrency != 0 ? concurrency : hw_concurrency;
    if (num_cycles == 0 && num_packets == 0) {
        LOG(log,0) << "simulating until drained" << endl;
    } else {
        ostringstream oss;
        if (num_cycles > 0) {
            oss << " for " << dec << num_cycles
                << " cycle" << (num_cycles == 1 ? "" : "s");
        }
        if (num_cycles > 0 && num_packets >> 0) {
            oss << " or";
        }
        if (num_packets > 0) {
            oss << " until " << dec << num_packets
                << " packet" << (num_packets == 1 ? "" : "s") << " arrive";
        }
        LOG(log,0) << "simulating" << oss.str() << endl;
    }
    if (num_threads == 1) {
        LOG(log,0) << "no concurrency";
    } else {
        LOG(log,0) << dec << num_threads << "-way concurrency, ";
        if (sync_period == 0) {
            LOG(log,0) << "cycle-accurate synchronization";
        } else {
            LOG(log,0) << "synchronization every " << dec << sync_period 
                << " cycle" << (sync_period == 1 ? "" : "s");
        }
    }
    LOG(log,0) << endl << endl;
    vector<tile_id> all_tile_ids;
    for (uint32_t tl = 0; tl < s->get_num_tiles(); ++tl) {
        all_tile_ids.push_back(tl);
    }
    if (tile_mapping == TM_RANDOM) {
        boost::function<int(int)> rr_fn =
            bind(&random_gen::random_range, rng, _1);
        random_shuffle(all_tile_ids.begin(), all_tile_ids.end(), rr_fn);
    }
    vector<vector<tile_id> > tiles_per_thread(num_threads);
    {
        uint64_t thr;
        uint64_t tile_index;
        vector<tile_id>::const_iterator ti;
        for (ti = all_tile_ids.begin(), thr = num_threads - 1, tile_index = 0;
             ti != all_tile_ids.end(); ++ti, ++tile_index) {
            switch (tile_mapping) {
            case TM_SEQUENTIAL:
                thr = tile_index * num_threads / all_tile_ids.size();
                break;
            case TM_ROUND_ROBIN:
            case TM_RANDOM:
                thr = (thr + 1) % num_threads;
                break;
            default: abort();
            }
            assert(thr < tiles_per_thread.size());
            tiles_per_thread[thr].push_back(*ti);
        }
    }
    sync_barrier = shared_ptr<barrier>(new barrier(num_threads));
    for (uint32_t i = 0; i < num_threads; ++i) {
        per_thread_drained.push_back(false);
        per_thread_time_exceeded.push_back(false);
        per_thread_packet_count.push_back(0);
        per_thread_next_time.push_back(0);
    }
    for (uint32_t i = 0; i < num_threads; ++i) {
        shared_ptr<vcd_writer> thr_vcd =
            i == num_threads - 1 ? vcd : shared_ptr<vcd_writer>();
        int cpu_affinity = -1;
        if (cpu_affinities.size() > 0) {
            cpu_affinity = cpu_affinities[i % cpu_affinities.size()];
        }
        shared_ptr<sim_thread> st = 
            shared_ptr<sim_thread>(new sim_thread(i, s, tiles_per_thread[i],
                                                  num_cycles, num_packets,
                                                  sync_period, sync_barrier,
                                                  global_drained,
                                                  global_next_time,
                                                  global_max_sync_count,
                                                  per_thread_drained,
                                                  per_thread_time_exceeded,
                                                  per_thread_packet_count,
                                                  per_thread_next_time,
                                                  enable_fast_forward,
                                                  cpu_affinity,
                                                  thr_vcd));
        sim_threads.push_back(st);
        try {
            threads.create_thread(*st);
        } catch (const thread_resource_error &e) {
            throw err_thread_spawn(string(e.what()));
        }
    }
}

sim::~sim() {
    threads.join_all();
}
