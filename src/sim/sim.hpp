// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SIM_HPP__
#define __SIM_HPP__

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/barrier.hpp>
#include "random.hpp"
#include "vcd.hpp"
#include "sys.hpp"
#include "logger.hpp"

using namespace std;
using namespace boost;

class sim_thread {
public:
    sim_thread(uint32_t thread_index,
               shared_ptr<sys> system,
               const vector<tile_id> &my_tiles,
               const uint64_t num_cycles,
               const uint64_t num_packets,
               const uint64_t sync_period,
               shared_ptr<barrier> sync_barrier,
               bool &global_drained,
               uint64_t &global_next_time,
               uint64_t &global_max_sync_count,
               vector<bool> &per_thread_drained,
               vector<bool> &per_thread_time_exceeded,
               vector<uint64_t> &per_thread_packet_count,
               vector<uint64_t> &per_thread_next_time,
               bool enable_fast_forward,
               int cpu_affinity, // -1 if none
               shared_ptr<vcd_writer> vcd
#ifdef PROGRESSIVE_STATISTICS_REPORT
               , shared_ptr<system_statistics> stats
#endif
               );
    void operator()();
private:
    uint32_t my_thread_index;
    shared_ptr<sys> s;
    const vector<tile_id> my_tile_ids;
    const uint64_t max_num_cycles;
    const uint64_t max_num_packets;
    const uint64_t sync_period;
    shared_ptr<barrier> sync_barrier;
    bool &global_drained; // written only by thread 0
    uint64_t &global_next_time; // written only by thread 0
    uint64_t &global_max_sync_count; // written only by thread 0
    vector<bool> &per_thread_drained;
    vector<bool> &per_thread_time_exceeded;
    vector<uint64_t> &per_thread_packet_count;
    vector<uint64_t> &per_thread_next_time;
    bool enable_fast_forward;
    int cpu; // CPU this thread should be pinned to; -1 if none
    shared_ptr<vcd_writer> vcd;
#ifdef PROGRESSIVE_STATISTICS_REPORT
    shared_ptr<system_statistics> stats;
#endif
};

class sim {
public:
    typedef enum { TM_SEQUENTIAL, TM_ROUND_ROBIN, TM_RANDOM } tile_mapping_t;
public:
    sim(shared_ptr<sys> system,
        const uint64_t num_cycles, const uint64_t num_packets,
        const uint64_t sync_period, const uint32_t concurrency,
        bool enable_fast_forward,
        tile_mapping_t tile_mapping,
        const vector<unsigned> &cpu_affinities,
        shared_ptr<vcd_writer> vcd, logger &log,
        shared_ptr<random_gen> rng
#ifdef PROGRESSIVE_STATISTICS_REPORT
        , shared_ptr<system_statistics> stats
#endif
        );
    virtual ~sim();
private:
    vector<shared_ptr<sim_thread> > sim_threads;
    thread_group threads;
    shared_ptr<barrier> sync_barrier;
    bool global_drained; // written only by thread 0
    uint64_t global_next_time; // written only by thread 0
    uint64_t global_max_sync_count; // written only by thread 0
    vector<bool> per_thread_drained;
    vector<bool> per_thread_time_exceeded;
    vector<uint64_t> per_thread_packet_count;
    vector<uint64_t> per_thread_next_time;
    logger &log;
    shared_ptr<random_gen> rng;
};

#endif // __SIM_HPP__
