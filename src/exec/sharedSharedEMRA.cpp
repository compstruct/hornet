// -*- mode:c++; c-style:k&r; c-basic-offset:5; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "sharedSharedEMRA.hpp"
#include "messages.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>

#define PRINT_PROGRESS
//#undef PRINT_PROGRESS

#define DEADLOCK_CHECK
#undef DEADLOCK_CHECK

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(true) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

static void l1_invalidation_hook(cacheLine &line) {
    shared_ptr<sharedSharedEMRA::coherenceInfo> info = 
        static_pointer_cast<sharedSharedEMRA::coherenceInfo>(line.coherence_info);
    *(info->in_l1) = false;
}

static shared_ptr<void> copy_coherence_info(shared_ptr<void> source) { 
    shared_ptr<sharedSharedEMRA::coherenceInfo> ret
        (new sharedSharedEMRA::coherenceInfo(*static_pointer_cast<sharedSharedEMRA::coherenceInfo>(source)));
    return ret;
}

static void cache_reserve_line(cacheLine &line) { 
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<sharedSharedEMRA::coherenceInfo>(new sharedSharedEMRA::coherenceInfo);
    }
    return; 
}

static bool l2_can_evict_line_inclusive(cacheLine &line, const uint64_t& system_time) {
    shared_ptr<sharedSharedEMRA::coherenceInfo> info = 
        static_pointer_cast<sharedSharedEMRA::coherenceInfo>(line.coherence_info);
    return !*(info->in_l1);
}

sharedSharedEMRA::sharedSharedEMRA(uint32_t id, 
                                   const uint64_t &t, 
                                   shared_ptr<tile_statistics> st, 
                                   logger &l, 
                                   shared_ptr<random_gen> r, 
                                   shared_ptr<cat> a_cat, 
                                   sharedSharedEMRACfg_t cfg) :
    memory(id, t, st, l, r), 
    m_cfg(cfg), 
    m_l1(NULL), 
    m_l2(NULL), 
    m_cat(a_cat), 
    m_stats(shared_ptr<sharedSharedEMRAStatsPerTile>()),
    m_work_table_vacancy(cfg.work_table_size),
    m_available_core_ports(cfg.num_local_core_ports)
{
    if (m_cfg.bytes_per_flit == 0) throw err_bad_shmem_cfg("flit size must be non-zero.");
    if (m_cfg.words_per_cache_line == 0) throw err_bad_shmem_cfg("cache line size must be non-zero.");
    if (m_cfg.lines_in_l1 == 0) throw err_bad_shmem_cfg("sharedSharedEMRA : L1 size must be non-zero.");
    if (m_cfg.lines_in_l2 == 0) throw err_bad_shmem_cfg("sharedSharedEMRA : L2 size must be non-zero.");
    if (m_cfg.work_table_size == 0) 
        throw err_bad_shmem_cfg("sharedSharedEMRA : work table must be non-zero.");
    if (m_cfg.work_table_size <= m_cfg.num_local_core_ports) 
        throw err_bad_shmem_cfg("sharedSharedEMRA : work table size must be greater than local core ports.");

    replacementPolicy_t l1_policy, l2_policy;

    switch (cfg.l1_replacement_policy) {
    case _REPLACE_LRU:
        l1_policy = REPLACE_LRU;
        break;
    case _REPLACE_RANDOM:
    default:
        l1_policy = REPLACE_RANDOM;
        break;
    }

    switch (cfg.l2_replacement_policy) {
    case _REPLACE_LRU:
        l2_policy = REPLACE_LRU;
        break;
    case _REPLACE_RANDOM:
    default:
        l2_policy = REPLACE_RANDOM;
        break;
    }

    m_l1 = new cache(1, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l1, cfg.l1_associativity, l1_policy,
                     cfg.l1_hit_test_latency, cfg.l1_num_read_ports, cfg.l1_num_write_ports);
    m_l2 = new cache(2, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l2, cfg.l2_associativity, l2_policy,
                     cfg.l2_hit_test_latency, cfg.l2_num_read_ports, cfg.l2_num_write_ports);

    m_l1->set_helper_copy_coherence_info(&copy_coherence_info);
    m_l1->set_helper_reserve_line(&cache_reserve_line);
    m_l1->set_helper_invalidate(&l1_invalidation_hook);

    m_l2->set_helper_copy_coherence_info(&copy_coherence_info);
    m_l2->set_helper_reserve_line(&cache_reserve_line);

    m_l2->set_helper_can_evict_line(&l2_can_evict_line_inclusive);

}

sharedSharedEMRA::~sharedSharedEMRA() {
    delete m_l1;
    delete m_l2;
}

uint32_t sharedSharedEMRA::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

void sharedSharedEMRA::request(shared_ptr<memoryRequest> req) {

    /* assumes a request is not across multiple cache lines */
    uint32_t __attribute__((unused)) byte_offset = req->maddr().address%(m_cfg.words_per_cache_line*4);
    mh_assert( (byte_offset + req->word_count()*4) <= m_cfg.words_per_cache_line * 4);

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    m_core_port_schedule_q.push_back(req);

}

void sharedSharedEMRA::tick_positive_edge() {
    /* schedule and make requests */
#ifdef PRINT_PROGRESS
    static uint64_t last_served[64];
    if (system_time % 10000 == 0) {
        cerr << "[MEM " << m_id << " @ " << system_time << " ]";
        if (stats_enabled()) {
            cerr << " total served : " << stats()->total_served();
            cerr << " since last : " << stats()->total_served() - last_served[m_id];
            last_served[m_id] = stats()->total_served();
        }
        cerr << " in work table : " << m_work_table.size() << endl;
    }
#endif

    schedule_requests();

    m_l1->tick_positive_edge();
    m_l2->tick_positive_edge();
    m_cat->tick_positive_edge();
    if(m_dram_controller) {
        m_dram_controller->tick_positive_edge();
    }
}

void sharedSharedEMRA::tick_negative_edge() {

    m_l1->tick_negative_edge();
    m_l2->tick_negative_edge();
    m_cat->tick_negative_edge();
    if(m_dram_controller) {
        m_dram_controller->tick_negative_edge();
    }

    /* accept messages and write into tables */
    accept_incoming_messages();

    work_table_update();

    dram_work_table_update();

}

void sharedSharedEMRA::apply_breakdown_info(shared_ptr<breakdownInfo> breakdown) {
    if (stats_enabled()) {
        stats()->add_memory_subsystem_serialization_cost(breakdown->mem_serialization);
        stats()->add_cat_serialization_cost(breakdown->cat_serialization);
        stats()->add_cat_action_cost(breakdown->cat_action);
        stats()->add_l1_serialization_cost(breakdown->l1_serialization);
        stats()->add_l1_action_cost(breakdown->l1_action);
        stats()->add_ra_req_network_plus_serialization_cost(breakdown->ra_req_network_plus_serialization);
        stats()->add_ra_rep_network_plus_serialization_cost(breakdown->ra_rep_network_plus_serialization);
        stats()->add_l2_serialization_cost(breakdown->l2_serialization);
        stats()->add_l2_action_cost(breakdown->l2_action);
        stats()->add_dram_req_onchip_network_plus_serialization_cost(breakdown->dram_req_onchip_network_plus_serialization);
        stats()->add_dram_rep_onchip_network_plus_serialization_cost(breakdown->dram_rep_onchip_network_plus_serialization);
        stats()->add_dram_offchip_network_plus_dram_action_cost(breakdown->dram_offchip);
    }
}

void sharedSharedEMRA::work_table_update() {
    for (workTable::iterator it_addr = m_work_table.begin(); it_addr != m_work_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        shared_ptr<tableEntry> entry = it_addr->second;

#if 0
        mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] in state " << entry->status 
                  << " for " << start_maddr << endl;
#endif

        shared_ptr<memoryRequest> core_req = entry->core_req;
        shared_ptr<coherenceMsg> data_req = entry->data_req;;
        shared_ptr<catRequest> cat_req = entry->cat_req;
        shared_ptr<cacheRequest> l1_req = entry->l1_req;
        shared_ptr<cacheRequest> l2_req = entry->l2_req;
        shared_ptr<coherenceMsg> data_rep = entry->data_rep; 
        shared_ptr<message_t> msg_to_send = entry->net_msg_to_send;
        shared_ptr<dramMsg> dram_req = entry->dram_req;
        shared_ptr<dramMsg> dram_rep = entry->dram_rep;

        shared_ptr<cacheLine> l1_line = (l1_req)? l1_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> l2_line = (l2_req)? l2_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> l2_victim = (l2_req)? l2_req->victim_line_copy() : shared_ptr<cacheLine>();

        shared_ptr<coherenceInfo> l1_line_info = 
            l1_line ? static_pointer_cast<coherenceInfo>(l1_line->coherence_info) : shared_ptr<coherenceInfo>();
        shared_ptr<coherenceInfo> l2_line_info = 
            l2_line ? static_pointer_cast<coherenceInfo>(l2_line->coherence_info) : shared_ptr<coherenceInfo>();
        shared_ptr<coherenceInfo> l2_victim_info = 
            l2_victim ? static_pointer_cast<coherenceInfo>(l2_victim->coherence_info) : shared_ptr<coherenceInfo>();

        shared_ptr<breakdownInfo> breakdown = entry->breakdown;

        if (entry->status == _WORK_WAIT_CAT_AND_L1_FOR_LOCAL_READ) {
            /* READ */
            if (l1_req->status() == CACHE_REQ_HIT) {

                /* it's for sure that it get's a core hit */
                entry->cat_req = shared_ptr<catRequest>();

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l1_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);
                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a read HIT and finish serving address "
                          << core_req->maddr() << endl;

                if (stats_enabled()) {
                    breakdown->cat_serialization = breakdown->temp_cat_serialization = 0;
                    breakdown->cat_action = breakdown->temp_cat_action = 0;
                    breakdown->l1_serialization = breakdown->temp_l1_serialization;
                    breakdown->l1_action += system_time - l1_req->milestone_time();
                    breakdown->temp_l1_serialization = 0;
                    breakdown->temp_l1_action = 0;

                    apply_breakdown_info(breakdown);
                    stats()->did_read_l1(true);
                    stats()->did_finish_read(system_time - entry->requested_time);
                }
                ++m_work_table_vacancy;
                m_work_table.erase(it_addr++);
                continue;

            } else {
                
                if (cat_req->milestone_time() != UINT64_MAX && cat_req->status() == CAT_REQ_DONE) {
                    breakdown->temp_cat_action += system_time - cat_req->milestone_time();
                    cat_req->set_milestone_time(UINT64_MAX);
                } 

                if (l1_req->milestone_time() != UINT64_MAX && l1_req->status() == CACHE_REQ_MISS) {
                    breakdown->temp_l1_action += system_time - l1_req->milestone_time();
                    l1_req->set_milestone_time(UINT64_MAX);
                }

                if (cat_req->status() != CAT_REQ_DONE) {
                    /* we can't continue without home information */
                    ++it_addr;
                    continue;
                }

                uint32_t home = cat_req->home();

                if (home != m_id) {

                    if (stats_enabled()) {
                        stats()->did_read_cat(false);
                    }

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a read core MISS for "
                              << core_req->maddr() << endl;

                    /* CAT is the critical path */
                    entry->l1_req = shared_ptr<cacheRequest>();
                    breakdown->l1_serialization = breakdown->temp_l1_serialization = 0;
                    breakdown->l1_action = breakdown->temp_l1_action = 0;
                    breakdown->cat_serialization = breakdown->temp_cat_serialization;
                    breakdown->cat_action = breakdown->temp_cat_action;
                    breakdown->temp_cat_serialization = 0;
                    breakdown->temp_cat_action = 0;

                    if (m_cfg.logic == MIGRATION_ALWAYS) {
                        if (stats_enabled()) {
                            apply_breakdown_info(breakdown);
                        }
                        set_req_status(core_req, REQ_MIGRATE);
                        set_req_home(core_req, home);
                        ++m_available_core_ports;
                        ++m_work_table_vacancy;
                        m_work_table.erase(it_addr++);
                        continue;
                    }

                    data_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                    data_req->sender = m_id;
                    data_req->receiver = home;
                    data_req->type = DATA_READ_REQ;
                    data_req->word_count = core_req->word_count();
                    data_req->maddr = core_req->maddr();
                    data_req->data = shared_array<uint32_t>();
                    data_req->did_win_last_arbitration = false;
                    data_req->waited = 0;
                    data_req->milestone_time = system_time;
                    data_req->breakdown = breakdown;

                    entry->data_req = data_req;

                    msg_to_send = shared_ptr<message_t>(new message_t);
                    msg_to_send->src = m_id;
                    msg_to_send->dst = home;
                    msg_to_send->type = MSG_DATA_REQ;
                    msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                    msg_to_send->content = data_req;
                    entry->net_msg_to_send = msg_to_send;
                    m_to_network_schedule_q[msg_to_send->type].push_back(msg_to_send);

                    entry->status = _WORK_SEND_REMOTE_DATA_REQ;

                } else if (l1_req->status() == CACHE_REQ_MISS) {

                    if (stats_enabled()) {
                        stats()->did_read_cat(true);
                        stats()->did_read_l1(false);
                    }

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a core hit but a read L1 MISS for "
                              << core_req->maddr() << endl;

                    if (breakdown->temp_cat_serialization + breakdown->temp_cat_action < 
                        breakdown->temp_l1_serialization + breakdown->temp_l1_action) 
                    {
                        breakdown->cat_serialization = breakdown->temp_cat_serialization = 0;
                        breakdown->cat_action = breakdown->temp_cat_action = 0;
                        breakdown->l1_serialization = breakdown->temp_l1_serialization;
                        breakdown->l1_action = breakdown->temp_l1_action;
                        breakdown->temp_l1_serialization = 0;
                        breakdown->temp_l1_action = 0;
                    } else {
                        breakdown->l1_serialization = breakdown->temp_l1_serialization = 0;
                        breakdown->l1_action = breakdown->temp_l1_action = 0;
                        breakdown->cat_serialization = breakdown->temp_cat_serialization;
                        breakdown->cat_action = breakdown->temp_cat_action;
                        breakdown->temp_cat_serialization = 0;
                        breakdown->temp_cat_action = 0;
                    }

                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_READ,
                                                                       m_cfg.words_per_cache_line));
                    l2_req->set_milestone_time(system_time);
                    l2_req->set_stats_info(breakdown);

                    entry->l2_req = l2_req;

                    entry->status = _WORK_WAIT_L2_FOR_LOCAL;
                }
                ++it_addr;
                continue;
            }

        } else if (entry->status == _WORK_WAIT_CAT_AND_L1_FOR_LOCAL_WRITE) {

            if (cat_req->milestone_time() != UINT64_MAX && cat_req->status() == CAT_REQ_DONE) {
                breakdown->temp_cat_action += system_time - cat_req->milestone_time();
                cat_req->set_milestone_time(UINT64_MAX);
            } 

            if (l1_req->milestone_time() != UINT64_MAX  
                && (l1_req->status() == CACHE_REQ_MISS || l1_req->status() == CACHE_REQ_HIT)
               ) 
            {
                breakdown->temp_l1_action += system_time - l1_req->milestone_time();
                l1_req->set_milestone_time(UINT64_MAX);
            }

            if (l1_req->status() == CACHE_REQ_HIT) {

                /* We will deal with l1 hit statistics at the end */

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a write hit at L1 for "
                          << core_req->maddr() << endl;

                entry->cat_req = shared_ptr<catRequest>();

                l2_req = shared_ptr<cacheRequest>(new cacheRequest(core_req->maddr(), CACHE_REQ_WRITE,
                                                                   core_req->word_count(), core_req->data()));
                l2_req->set_milestone_time(system_time);
                l2_req->set_stats_info(breakdown);

                entry->l2_req = l2_req;

                entry->status = _WORK_WAIT_L2_FOR_LOCAL;
                ++it_addr;
                continue;
            }

            if (cat_req->status() != CAT_REQ_DONE) {
                ++it_addr;
                continue;
            }

            uint32_t home = cat_req->home();

            if (home == m_id) {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a write core hit so writing through for "
                          << core_req->maddr() << endl;
                l2_req = shared_ptr<cacheRequest>(new cacheRequest(core_req->maddr(), CACHE_REQ_WRITE,
                                                                   core_req->word_count(), core_req->data()));
                l2_req->set_milestone_time(system_time);
                l2_req->set_stats_info(breakdown);

                entry->l2_req = l2_req;
                
                entry->status = _WORK_WAIT_L2_FOR_LOCAL;
                ++it_addr;
                continue;
 
            } else {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a write core MISS for "
                          << core_req->maddr() << endl;

                entry->l1_req = shared_ptr<cacheRequest>();
                breakdown->l1_serialization = breakdown->temp_l1_serialization = 0;
                breakdown->l1_action = breakdown->temp_l1_action = 0;
                breakdown->cat_serialization = breakdown->temp_cat_serialization;
                breakdown->cat_action = breakdown->temp_cat_action;
                breakdown->temp_cat_serialization = 0;
                breakdown->temp_cat_action = 0;

                if (stats_enabled()) {
                    stats()->did_read_cat(false);
                }
                
                if (m_cfg.logic == MIGRATION_ALWAYS) {
                    if (stats_enabled()) {
                        apply_breakdown_info(breakdown);
                    }
                    set_req_status(core_req, REQ_MIGRATE);
                    set_req_home(core_req, home);
                    ++m_available_core_ports;
                    ++m_work_table_vacancy;
                    m_work_table.erase(it_addr++);
                    continue;
                }

                data_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                data_req->sender = m_id;
                data_req->receiver = home;
                data_req->type = DATA_WRITE_REQ;
                data_req->word_count = core_req->word_count();
                data_req->maddr = core_req->maddr();
                data_req->data = core_req->data();
                data_req->did_win_last_arbitration = false;
                data_req->waited = 0;
                data_req->milestone_time = system_time;
                data_req->breakdown = breakdown;

                entry->data_req = data_req;

                msg_to_send = shared_ptr<message_t>(new message_t);
                msg_to_send->src = m_id;
                msg_to_send->dst = home;
                msg_to_send->type = MSG_DATA_REQ;
                msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + 4*core_req->word_count());
                msg_to_send->content = data_req;
                entry->net_msg_to_send = msg_to_send;
                m_to_network_schedule_q[msg_to_send->type].push_back(msg_to_send);

                entry->status = _WORK_SEND_REMOTE_DATA_REQ;

                ++it_addr;
                continue;
            }
        } else if (entry->status == _WORK_SEND_REMOTE_DATA_REQ) {

            if (data_req->did_win_last_arbitration) {
                data_req->did_win_last_arbitration = false;
                entry->net_msg_to_send = shared_ptr<message_t>();
                entry->status = _WORK_WAIT_REMOTE_DATA_REP;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent a data request (" << data_req->type
                          << ") to " << data_req->receiver << " for " << data_req->maddr << endl;
            } else {
                m_to_network_schedule_q[msg_to_send->type].push_back(msg_to_send);
            }
            ++it_addr;
            continue;

        } else if (entry->status == _WORK_WAIT_REMOTE_DATA_REP) {

            if (data_rep) {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a data reply (" << data_rep->type
                    << ") from " << data_rep->sender << " for " << data_rep->maddr << endl;

                /* cost breakdown study */
                breakdown->ra_rep_network_plus_serialization += system_time - data_rep->milestone_time;

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = data_rep->data[i + word_offset];
                }
                set_req_data(core_req, ret);

                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;
                ++m_work_table_vacancy;

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finished serving address "
                    << core_req->maddr() << " by remote accesses" << endl;

                if (stats_enabled()) {
                    apply_breakdown_info(breakdown);

                    if (core_req->is_read()) {
                        stats()->did_finish_read(system_time - entry->requested_time);
                    } else {
                        stats()->did_finish_write(system_time - entry->requested_time);
                    }
                }
                m_work_table.erase(it_addr++);
                continue;
            }
            ++it_addr;
            continue;

        } else if (entry->status == _WORK_WAIT_L2_FOR_LOCAL) {

            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            breakdown->temp_l2_action += system_time - l2_req->milestone_time();

            if (l2_req->status() == CACHE_REQ_MISS) {

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 MISS for a local request on "
                          << core_req->maddr() << endl;

                l1_req = shared_ptr<cacheRequest>();

                if (l2_line) {
                    if (!core_req->is_read()) {
                        if (stats_enabled()) {
                            stats()->did_write_l1(false);
                            stats()->did_write_l2(false);
                            stats()->did_read_cat(true);
                        }
                        breakdown->l1_serialization = breakdown->temp_l1_serialization = 0;
                        breakdown->l1_action = breakdown->temp_l1_action = 0;
                        breakdown->cat_serialization = breakdown->temp_cat_serialization;
                        breakdown->cat_action = breakdown->temp_cat_action;
                        breakdown->temp_cat_serialization = 0;
                        breakdown->temp_cat_action = 0;
                    } else {
                        if (stats_enabled()) {
                            stats()->did_read_l2(false);
                        }
                    }
                    breakdown->l2_serialization = breakdown->temp_l2_serialization;
                    breakdown->l2_action = breakdown->temp_l2_action;
                    breakdown->temp_l2_serialization = 0;
                    breakdown->temp_l2_action = 0;

                    if (l2_victim && l2_victim->dirty) {

                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicts a line of " << l2_victim->start_maddr
                                  << " and writing back to DRAM for a local request on " << start_maddr << endl;

                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                DRAM_REQ_WRITE,
                                                                                m_cfg.words_per_cache_line,
                                                                                l2_victim->data));
                        dram_req->did_win_last_arbitration = false;
                        dram_req->milestone_time = system_time;
                        dram_req->breakdown = breakdown;

                        entry->dram_req = dram_req;

                        if (m_dram_controller_location == m_id) {
                            m_to_dram_writeback_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dram_controller_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = 
                                get_flit_count (1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                            entry->net_msg_to_send->content = dram_req;

                            m_to_network_schedule_q_priority[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }
                        entry->status = _WORK_DRAM_WRITEBACK_AND_FEED;
                    } else {
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] is sending a DRAM request for a local request on "
                                  << start_maddr << endl;

                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                DRAM_REQ_READ,
                                                                                m_cfg.words_per_cache_line));
                        dram_req->did_win_last_arbitration = false;
                        entry->dram_req = dram_req;
                        dram_req->milestone_time = system_time;
                        dram_req->breakdown = breakdown;

                        if (m_dram_controller_location == m_id) {
                            m_to_dram_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dram_controller_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes);
                            entry->net_msg_to_send->content = dram_req;
                            m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }
                        entry->status = _WORK_SEND_DRAM_FEED_REQ;
                    }
                } else {
                    if (l2_victim) {
                        /* could not evict */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] could not evict a line of " << l2_victim->start_maddr
                            << " for  a local request on " << start_maddr << endl;
                        if (entry->l1_evict_req) {
                            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] last invalidate request status " << entry->l1_evict_req->status() << endl;
                        }
                        if (m_work_table.count(l2_victim->start_maddr) == 0 &&
                            (!entry->l1_evict_req || entry->l1_evict_req->status() == CACHE_REQ_MISS || entry->l1_evict_req->status() == CACHE_REQ_HIT)) 
                        {
                            entry->l1_evict_req = shared_ptr<cacheRequest>(new cacheRequest(l2_victim->start_maddr, CACHE_REQ_INVALIDATE));
                            entry->l1_evict_req->set_reserve(false);
                            entry->l1_evict_req->set_milestone_time(UINT64_MAX);
                            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] invalidate requested for a line of " << l2_victim->start_maddr
                                << " for  a local request on " << start_maddr << endl;
                        }
                    }
                    l2_req->set_milestone_time(system_time);
                    l2_req->reset();
                }
                ++it_addr;
                continue;

            } else {
                /* HIT */
                if (stats_enabled()) {
                    if (core_req->is_read()) {
                        stats()->did_read_l2(true);
                    } else {
                        stats()->did_write_l2(true);
                    }
                }

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 HIT for a local request on "
                          << start_maddr << endl;

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 )  % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l2_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);

                if (core_req->is_read()) {

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] updates a cache line for a local read request on "
                              << start_maddr << endl;

                    breakdown->l2_serialization = breakdown->temp_l2_serialization;
                    breakdown->l2_action = breakdown->temp_l2_action;
                    breakdown->temp_l2_serialization = 0;
                    breakdown->temp_l2_action = 0;

                    l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                       m_cfg.words_per_cache_line, 
                                                                       l2_line->data, 
                                                                       l2_line_info));

                    l1_req->set_stats_info(breakdown);
                    *(l2_line_info->in_l1) = true;
                    l1_req->set_milestone_time(system_time);

                    entry->l1_req = l1_req;
                    entry->status = _WORK_UPDATE_L1;
                    ++it_addr;
                    continue;

                } else {

                    if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                        /* L1 latency hides all others */
                        breakdown->cat_serialization = breakdown->temp_cat_serialization = 0;
                        breakdown->cat_action = breakdown->temp_cat_action = 0;
                        breakdown->l2_serialization = breakdown->temp_l2_serialization = 0;
                        breakdown->l2_action = breakdown->temp_l2_action = 0;
                        entry->status = _WORK_WAIT_L1_WRITE_AFTER_L2;
                        ++it_addr;

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] L2 gets a write hit "
                                  << "but now waiting for the L1 for a local request on "
                                  << start_maddr << endl;

                        continue;
                    }

                    /* L1 latency is hidden */
                    breakdown->l1_serialization = breakdown->temp_l1_serialization = 0;
                    breakdown->l1_action = breakdown->temp_l1_action = 0;
                    breakdown->cat_serialization = breakdown->temp_cat_serialization;
                    breakdown->cat_action = breakdown->temp_cat_action;
                    breakdown->l2_serialization = breakdown->temp_l2_serialization;
                    breakdown->l2_action = breakdown->temp_l2_action;
                    breakdown->temp_cat_serialization = 0;
                    breakdown->temp_cat_action = 0;
                    breakdown->temp_l2_serialization = 0;
                    breakdown->temp_l2_action = 0;


                    if (l1_req->status() == CACHE_REQ_HIT) {

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] Both L1 and L2 gets a write hit "
                                  << "for a local request on "
                                  << start_maddr << " and finish serving " << endl;

                        if (stats_enabled()) {
                            apply_breakdown_info(breakdown);
                            stats()->did_write_l1(true);
                            stats()->did_finish_write(system_time - entry->requested_time);
                        }

                        set_req_status(core_req, REQ_DONE);
                        ++m_work_table_vacancy;
                        ++m_available_core_ports;

                        m_work_table.erase(it_addr++);
                        continue;

                    } else if (l1_req->status() == CACHE_REQ_MISS) {

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] updates a cache line for a local write request on "
                                  << start_maddr << endl;

                        if (stats_enabled()) {
                            stats()->did_write_l1(false);
                            stats()->did_read_cat(true);
                        }

                        l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                           m_cfg.words_per_cache_line, 
                                                                           l2_line->data, 
                                                                           l2_line_info));
                        *(l2_line_info->in_l1) = true;
                        l1_req->set_stats_info(breakdown);
                        l1_req->set_milestone_time(system_time);

                        entry->l1_req = l1_req;
                        entry->status = _WORK_UPDATE_L1;

                    }
                    ++it_addr;
                    continue;
                }
            }
        } else if (entry->status == _WORK_WAIT_L1_WRITE_AFTER_L2) {

            if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            breakdown->temp_l1_action += system_time - l1_req->milestone_time();
            breakdown->l1_serialization = breakdown->temp_l1_serialization;
            breakdown->l1_action = breakdown->temp_l1_action;
            breakdown->temp_l1_serialization = 0;
            breakdown->temp_l1_action = 0;

            if (l1_req->status() == CACHE_REQ_HIT) {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] Both L1 and L2 gets a write hit "
                    << "for a local request on "
                    << start_maddr << " and finish serving " << endl;

                if (stats_enabled()) {
                    apply_breakdown_info(breakdown);
                    stats()->did_write_l1(true);
                    stats()->did_finish_write(system_time - entry->requested_time);
                }

                set_req_status(core_req, REQ_DONE);
                ++m_work_table_vacancy;
                ++m_available_core_ports;

                m_work_table.erase(it_addr++);
                continue;

            } else {

                if (stats_enabled()) {
                    stats()->did_write_l1(false);
                    stats()->did_read_cat(true);
                }

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] updates a cache line for a local write request on "
                          << start_maddr << endl;

                l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line, 
                                                                   l2_line->data, l2_line_info));

                *(l2_line_info->in_l1) = true;
                l1_req->set_stats_info(breakdown);
                l1_req->set_milestone_time(system_time);

                entry->l1_req = l1_req;
                entry->status = _WORK_UPDATE_L1;
                ++it_addr;
                continue;
            }

        } else if (entry->status == _WORK_DRAM_WRITEBACK_AND_FEED) {

            if (dram_req->did_win_last_arbitration) {
                dram_req->did_win_last_arbitration = false;
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }

                breakdown->dram_req_onchip_network_plus_serialization += system_time - dram_req->milestone_time;

                dram_req = shared_ptr<dramMsg>(new dramMsg);
                dram_req->sender = m_id;
                dram_req->receiver = m_dram_controller_location;
                dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                        DRAM_REQ_READ,
                                                                        m_cfg.words_per_cache_line));
                dram_req->did_win_last_arbitration = false;
                dram_req->milestone_time = system_time;
                dram_req->breakdown = breakdown;

                entry->dram_req = dram_req;

                if (m_dram_controller_location == m_id) {
                    m_to_dram_req_schedule_q.push_back(entry->dram_req);
                } else {
                    entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                    entry->net_msg_to_send->src = m_id;
                    entry->net_msg_to_send->dst = m_dram_controller_location;
                    entry->net_msg_to_send->type = MSG_DRAM_REQ;
                    entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes);
                    entry->net_msg_to_send->content = dram_req;
                    m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                }
                entry->status = _WORK_SEND_DRAM_FEED_REQ;

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] is sending a DRAM request for a request on "
                    << start_maddr << " after a writeback " << endl;

            } else if (m_dram_controller_location == m_id) {
                m_to_dram_writeback_req_schedule_q.push_back(entry->dram_req);
            } else {
                m_to_network_schedule_q_priority[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
            }
            ++it_addr;
            continue;

        } else if (entry->status == _WORK_SEND_DRAM_FEED_REQ) {

            if (dram_req->did_win_last_arbitration) {
                dram_req->did_win_last_arbitration = false;
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a DRAM request for "
                          << dram_req->req->maddr() << " to " << dram_req->receiver << endl;
                entry->status = _WORK_WAIT_DRAM_FEED;
            } else if (m_dram_controller_location == m_id) {
                m_to_dram_req_schedule_q.push_back(entry->dram_req);
            } else {
                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
            }
            ++it_addr;
            continue;

        } else if (entry->status == _WORK_WAIT_DRAM_FEED) {

            if (dram_rep) {
                breakdown->dram_rep_onchip_network_plus_serialization += system_time - dram_rep->milestone_time;

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] received a DRAM reply for "
                    << dram_rep->req->maddr() << endl;


                if (core_req) {

                    l2_line_info = shared_ptr<coherenceInfo>(new coherenceInfo);
                    l2_line_info->in_l1 = shared_ptr<bool>(new bool(true));

                    if (!core_req->is_read()) {
                        uint32_t word_offset = (core_req->maddr().address / 4 )  % m_cfg.words_per_cache_line;
                        for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                            dram_rep->req->read()[i + word_offset] = core_req->data()[i + word_offset];
                        }
                    }

                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                       m_cfg.words_per_cache_line, 
                                                                       dram_rep->req->read(),
                                                                       l2_line_info));
                    l2_req->set_milestone_time(system_time);
                    l2_req->set_clean_write(core_req->is_read());
                    l2_req->set_stats_info(breakdown);
                    entry->l2_req = l2_req;


                    l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                       m_cfg.words_per_cache_line, 
                                                                       dram_rep->req->read(),
                                                                       l2_line_info));
                    l1_req->set_milestone_time(system_time);
                    l1_req->set_stats_info(breakdown);
                    entry->l1_req = l1_req;

                    entry->status = _WORK_UPDATE_L1_AND_L2;
                } else {
                    bool is_read = data_req->type == DATA_READ_REQ;

                    l2_line_info = shared_ptr<coherenceInfo>(new coherenceInfo);
                    l2_line_info->in_l1 = shared_ptr<bool>(new bool(false));

                    if (!is_read) {
                        uint32_t word_offset = (data_req->maddr.address / 4 )  % m_cfg.words_per_cache_line;
                        for (uint32_t i = 0; i < data_req->word_count; ++i) {
                            dram_rep->req->read()[i + word_offset] = data_req->data[i + word_offset];
                        }
                    }

                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                       m_cfg.words_per_cache_line, 
                                                                       dram_rep->req->read(),
                                                                       l2_line_info));
                    l2_req->set_milestone_time(system_time);
                    l2_req->set_clean_write(is_read);
                    l2_req->set_stats_info(breakdown);
                    entry->l2_req = l2_req;

                    entry->status = _WORK_UPDATE_L2;
                }
            }
            ++it_addr;
            continue;

        } else if (entry->status == _WORK_UPDATE_L1) {

            if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            } 

            breakdown->temp_l1_action += system_time - l1_req->milestone_time();

            if (l1_req->status() == CACHE_REQ_MISS) {
                l1_req->reset();
                l1_req->set_milestone_time(system_time);
                ++it_addr;
                continue;
            }

            /* HIT */

            breakdown->l1_serialization += breakdown->temp_l1_serialization;
            breakdown->l1_action += breakdown->temp_l1_action;
            breakdown->temp_l1_serialization = 0;
            breakdown->temp_l1_action = 0;

            if (stats_enabled()) {
                apply_breakdown_info(breakdown);
                if (core_req->is_read()) {
                    stats()->did_finish_read(system_time - entry->requested_time);
                } else {
                    stats()->did_finish_write(system_time - entry->requested_time);
                }
            }

            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] is updated for a local request on "
                      << start_maddr << endl;

            set_req_status(core_req, REQ_DONE);
            ++m_available_core_ports;
            ++m_work_table_vacancy;
            m_work_table.erase(it_addr++);
            continue;

        } else if (entry->status == _WORK_UPDATE_L1_AND_L2) {

            if (l1_req->milestone_time() != UINT64_MAX && 
                (l1_req->status() == CACHE_REQ_MISS || l1_req->status() == CACHE_REQ_HIT))
            {
                breakdown->temp_l1_action += system_time - l1_req->milestone_time();
                l1_req->set_milestone_time(UINT64_MAX);
            }

            if (l2_req->milestone_time() != UINT64_MAX && 
                (l2_req->status() == CACHE_REQ_MISS || l2_req->status() == CACHE_REQ_HIT)) 
            {
                breakdown->temp_l2_action += system_time - l2_req->milestone_time();
                l2_req->set_milestone_time(UINT64_MAX);
            }

            if (l1_req->status() == CACHE_REQ_MISS) {
                mh_assert(l1_line);
                l1_req->set_milestone_time(system_time);
                l1_req->reset();
            }
            if (l2_req->status() == CACHE_REQ_MISS) {
                if (l2_line) {
                    if (l2_victim && l2_victim->dirty) {
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] is trying to DRAM writeback a victim line of " 
                            << l2_victim->start_maddr
                            << "  for a local request on " 
                            << start_maddr << endl;
                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                DRAM_REQ_WRITE,
                                                                                m_cfg.words_per_cache_line,
                                                                                l2_victim->data));
                        dram_req->did_win_last_arbitration = false;
                        dram_req->milestone_time = system_time;
                        dram_req->breakdown = breakdown;

                        entry->dram_req = dram_req;

                        if (m_dram_controller_location == m_id) {
                            m_to_dram_writeback_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dram_controller_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = 
                                get_flit_count (1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                            entry->net_msg_to_send->content = dram_req;

                            m_to_network_schedule_q_priority[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }
                        entry->status = _WORK_DRAM_WRITEBACK_AND_UPDATE_L1_AND_L2;
                    }
                } else {
                    if (l2_victim) {
                        /* could not evict */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] couldn't evict a line of " << l2_victim->start_maddr
                            << "  for a local request on " 
                            << start_maddr << endl;

                        if (m_work_table.count(l2_victim->start_maddr) == 0) {
                            entry->l1_evict_req = shared_ptr<cacheRequest>(new cacheRequest(l2_victim->start_maddr, CACHE_REQ_INVALIDATE));
                            entry->l1_evict_req->set_reserve(false);
                            entry->l1_evict_req->set_milestone_time(UINT64_MAX);
                        }
                    }
                    l2_req->set_milestone_time(system_time);
                    l2_req->reset();
                }
                l2_req->set_milestone_time(system_time);
                l2_req->reset();
            }

            if (l1_req->status() == CACHE_REQ_HIT && l2_req->status() == CACHE_REQ_HIT) {
                if (breakdown->temp_l1_serialization + breakdown->temp_l1_action > 
                    breakdown->temp_l2_serialization + breakdown->temp_l2_action + breakdown->temp_dram_req_onchip_network_plus_serialization)
                {
                    breakdown->l1_serialization += breakdown->temp_l1_serialization;
                    breakdown->l1_action += breakdown->temp_l1_action;
                } else {
                    breakdown->l2_serialization += breakdown->temp_l2_serialization;
                    breakdown->l2_action += breakdown->temp_l2_action;
                    breakdown->dram_req_onchip_network_plus_serialization += breakdown->temp_dram_req_onchip_network_plus_serialization;
                }

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 )  % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l2_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);

                if (stats_enabled()) {
                    apply_breakdown_info(breakdown);
                    if (core_req->is_read()) {
                        stats()->did_finish_read(system_time - entry->requested_time);
                    } else {
                        stats()->did_finish_write(system_time - entry->requested_time);
                    }
                }

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] both L1 and L2 are updated for a local request on "
                          << start_maddr << endl;

                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;
                ++m_work_table_vacancy;
                m_work_table.erase(it_addr++);

            } else {
                ++it_addr;
                continue;
            }

        } else if (entry->status == _WORK_DRAM_WRITEBACK_AND_UPDATE_L1_AND_L2) {

            if (dram_req->did_win_last_arbitration) {
                dram_req->did_win_last_arbitration = false;
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }

                breakdown->temp_dram_req_onchip_network_plus_serialization += system_time - dram_req->milestone_time;

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] the evicted line of " << l2_victim->start_maddr
                          << " is written back to DRAM for a local request on " 
                          << start_maddr << endl;

                entry->status = _WORK_UPDATE_L1_AND_L2;

            } else if (m_dram_controller_location == m_id) {
                m_to_dram_writeback_req_schedule_q.push_back(entry->dram_req);
            } else {
                m_to_network_schedule_q_priority[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
            }

            ++it_addr;
            continue;
        } else if (entry->status == _WORK_WAIT_L2_FOR_REMOTE_READ) {

            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            breakdown->temp_l2_action = system_time - l2_req->milestone_time();

            if (l2_req->status() == CACHE_REQ_HIT) {

                breakdown->l2_serialization += breakdown->temp_l2_serialization;
                breakdown->temp_l2_serialization = 0;
                breakdown->l2_action += breakdown->temp_l2_action;
                breakdown->temp_l2_action = 0;

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a read HIT for a remote request on "
                          << data_req->maddr << endl;

                if (stats_enabled()) {
                    stats()->did_read_l2(true);
                }

                data_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                data_rep->sender = m_id;
                data_rep->did_win_last_arbitration = false;
                data_rep->waited = 0;
                data_rep->type = DATA_REP;
                data_rep->receiver = data_req->sender;
                data_rep->word_count = data_req->word_count;
                data_rep->maddr = data_req->maddr;
                data_rep->data = l2_line->data;
                data_rep->milestone_time = system_time;

                entry->data_rep = data_rep;

                msg_to_send = shared_ptr<message_t>(new message_t);
                msg_to_send->type = MSG_DATA_REP;
                msg_to_send->src = m_id;
                msg_to_send->dst = data_rep->receiver;
                msg_to_send->flit_count = get_flit_count(1 + data_rep->word_count * 4);
                msg_to_send->content = data_rep;
                entry->net_msg_to_send = msg_to_send;

                m_to_network_schedule_q[MSG_DATA_REP].push_back(msg_to_send);

                entry->status = _WORK_SEND_REMOTE_DATA_REP;
                ++it_addr;
                continue;

            } else {
                /* MISS */
                if (l2_line) {
                    
                    breakdown->l2_serialization += breakdown->temp_l2_serialization;
                    breakdown->temp_l2_serialization = 0;
                    breakdown->l2_action += breakdown->temp_l2_action;
                    breakdown->temp_l2_action = 0;

                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a read MISS for a remote request on "
                              << data_req->maddr << endl;

                    if (stats_enabled()) {
                        stats()->did_read_l2(false);
                    }

                    if (l2_victim && l2_victim->dirty) {
                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                DRAM_REQ_WRITE,
                                                                                m_cfg.words_per_cache_line,
                                                                                l2_victim->data));
                        dram_req->did_win_last_arbitration = false;
                        dram_req->milestone_time = system_time;
                        dram_req->breakdown = breakdown;

                        entry->dram_req = dram_req;

                        if (m_dram_controller_location == m_id) {
                            m_to_dram_writeback_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dram_controller_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = 
                                get_flit_count (1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                            entry->net_msg_to_send->content = dram_req;

                            m_to_network_schedule_q_priority[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] is sending a DRAM writeback on "
                                  << l2_victim->start_maddr << " for a remote request on "
                                  << data_req->maddr << endl;
                        entry->status = _WORK_DRAM_WRITEBACK_AND_FEED;

                    } else {
                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                DRAM_REQ_READ,
                                                                                m_cfg.words_per_cache_line));
                        dram_req->did_win_last_arbitration = false;
                        entry->dram_req = dram_req;
                        dram_req->milestone_time = system_time;
                        dram_req->breakdown = breakdown;

                        if (m_dram_controller_location == m_id) {
                            m_to_dram_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dram_controller_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes);
                            entry->net_msg_to_send->content = dram_req;
                            m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }

                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] is sending a DRAM request for a remote read "
                                  << " request on "
                                  << data_req->maddr << endl;

                        entry->status = _WORK_SEND_DRAM_FEED_REQ;
                    }
                } else {
                    if (l2_victim) {
                        /* could not evict */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] could not evict a line of " << l2_victim->start_maddr
                            << " for a remote request on " 
                            << data_req->maddr << endl;
                        if (m_work_table.count(l2_victim->start_maddr) == 0) {
                            entry->l1_evict_req = shared_ptr<cacheRequest>(new cacheRequest(l2_victim->start_maddr, CACHE_REQ_INVALIDATE));
                            entry->l1_evict_req->set_reserve(false);
                            entry->l1_evict_req->set_milestone_time(UINT64_MAX);
                        }
                    }
                    l2_req->set_milestone_time(system_time);
                    l2_req->reset();
                }
                ++it_addr;
                continue;
            }
        } else if (entry->status == _WORK_WAIT_L1_AND_L2_FOR_REMOTE_WRITE) {

            if (l1_req && l1_req->milestone_time() != UINT64_MAX && 
                (l1_req->status() == CACHE_REQ_MISS || l1_req->status() == CACHE_REQ_HIT)) 
            {
                breakdown->temp_l1_action += system_time - l1_req->milestone_time();
                l1_req->set_milestone_time(UINT64_MAX);
            }

            if (l2_req->milestone_time() != UINT64_MAX && 
                (l2_req->status() == CACHE_REQ_MISS || l2_req->status() == CACHE_REQ_HIT)) 
            {
                breakdown->temp_l2_action += system_time - l2_req->milestone_time();
                l2_req->set_milestone_time(UINT64_MAX);
            }

            if (l2_req->status() == CACHE_REQ_HIT &&
                (l1_req->status() == CACHE_REQ_MISS || l1_req->status() == CACHE_REQ_HIT)) 
            {

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] finish a write through for a remote writ request on " 
                    << data_req->maddr << endl;

                if (breakdown->temp_l1_action + breakdown->temp_l1_serialization > 
                    breakdown->temp_l2_action + breakdown->temp_l2_serialization)
                {
                    breakdown->l1_serialization += breakdown->temp_l1_serialization;
                    breakdown->l1_action += breakdown->temp_l1_action;
                    breakdown->temp_l1_serialization = 0;
                    breakdown->temp_l1_action = 0;
                    breakdown->temp_l2_serialization = 0;
                    breakdown->temp_l2_action = 0;
                } else {
                    breakdown->l2_serialization += breakdown->temp_l2_serialization;
                    breakdown->l2_action += breakdown->temp_l2_action;
                    breakdown->temp_l2_serialization = 0;
                    breakdown->temp_l2_action = 0;
                    breakdown->temp_l1_serialization = 0;
                    breakdown->temp_l1_action = 0;
                }

                if (stats_enabled()) {
                    stats()->did_write_l2(true);
                }

                data_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                data_rep->sender = m_id;
                data_rep->did_win_last_arbitration = false;
                data_rep->waited = 0;
                data_rep->type = DATA_REP;
                data_rep->receiver = data_req->sender;
                data_rep->word_count = data_req->word_count;
                data_rep->maddr = data_req->maddr;
                data_rep->data = l2_line->data;
                data_rep->milestone_time = system_time;

                entry->data_rep = data_rep;

                msg_to_send = shared_ptr<message_t>(new message_t);
                msg_to_send->type = MSG_DATA_REP;
                msg_to_send->src = m_id;
                msg_to_send->dst = data_rep->receiver;
                msg_to_send->flit_count = get_flit_count(1);
                msg_to_send->content = data_rep;
                entry->net_msg_to_send = msg_to_send;

                m_to_network_schedule_q[MSG_DATA_REP].push_back(msg_to_send);

                entry->status = _WORK_SEND_REMOTE_DATA_REP;
                ++it_addr;
                continue;

            } else if (l2_req->status() == CACHE_REQ_MISS) {
                /* MISS */
                entry->l1_req = shared_ptr<cacheRequest>();

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 MISS for a remote write request on " 
                          << data_req->maddr << endl;

                if (l2_line) {

                    breakdown->temp_l1_serialization = 0;
                    breakdown->temp_l1_action = 0;
                    breakdown->l2_serialization = breakdown->temp_l2_serialization;
                    breakdown->l2_action = breakdown->temp_l2_action;
                    breakdown->temp_l2_serialization = 0;
                    breakdown->temp_l2_action = 0;

                    if (stats_enabled()) {
                        stats()->did_write_l2(false);
                    }

                    if (l2_victim && l2_victim->dirty) {
                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                DRAM_REQ_WRITE,
                                                                                m_cfg.words_per_cache_line,
                                                                                l2_victim->data));
                        dram_req->did_win_last_arbitration = false;
                        dram_req->milestone_time = system_time;
                        dram_req->breakdown = breakdown;

                        entry->dram_req = dram_req;

                        if (m_dram_controller_location == m_id) {
                            m_to_dram_writeback_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dram_controller_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = 
                                get_flit_count (1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                            entry->net_msg_to_send->content = dram_req;

                            m_to_network_schedule_q_priority[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sends a DRAM writeback of line of " 
                            << l2_victim->start_maddr << " for a remote write request on "  
                            << data_req->maddr << endl;
                        entry->status = _WORK_DRAM_WRITEBACK_AND_FEED;

                    } else {
                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                DRAM_REQ_READ,
                                                                                m_cfg.words_per_cache_line));
                        dram_req->did_win_last_arbitration = false;
                        entry->dram_req = dram_req;
                        dram_req->milestone_time = system_time;
                        dram_req->breakdown = breakdown;

                        if (m_dram_controller_location == m_id) {
                            m_to_dram_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dram_controller_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes);
                            entry->net_msg_to_send->content = dram_req;
                            m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }

                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sends a DRAM request "
                            << "for a remote write request on " 
                            << data_req->maddr << endl;

                        entry->status = _WORK_SEND_DRAM_FEED_REQ;
                    }
                } else {
                    if (l2_victim) {
                        /* could not evict */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] could not evict a line of " << l2_victim->start_maddr
                            << " for a remote write request on " 
                            << data_req->maddr << endl;

                        if (m_work_table.count(l2_victim->start_maddr) == 0) {
                            entry->l1_evict_req = shared_ptr<cacheRequest>(new cacheRequest(l2_victim->start_maddr, CACHE_REQ_INVALIDATE));
                            entry->l1_evict_req->set_reserve(false);
                            entry->l1_evict_req->set_milestone_time(UINT64_MAX);
                        }
                    }
                    l2_req->set_milestone_time(system_time);
                    l2_req->reset();
                }
                ++it_addr;
                continue;
            }

            ++it_addr;
            continue;


        } else if (entry->status == _WORK_UPDATE_L2) {
            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            breakdown->l2_serialization += breakdown->temp_l2_serialization;
            breakdown->temp_l2_serialization = 0;
            breakdown->l2_action += system_time - l2_req->milestone_time();

            if (l2_req->status() == CACHE_REQ_MISS) {
                l2_req->set_milestone_time(system_time);
                l2_req->reset();
                ++it_addr;
                continue;
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] is updated for a remote request on " 
                << data_req->maddr << endl;

            data_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
            data_rep->sender = m_id;
            data_rep->did_win_last_arbitration = false;
            data_rep->waited = 0;
            data_rep->type = DATA_REP;
            data_rep->receiver = data_req->sender;
            data_rep->word_count = data_req->word_count;
            data_rep->maddr = data_req->maddr;
            data_rep->data = l2_line->data;
            data_rep->milestone_time = system_time;

            entry->data_rep = data_rep;

            msg_to_send = shared_ptr<message_t>(new message_t);
            msg_to_send->type = MSG_DATA_REP;
            msg_to_send->src = m_id;
            msg_to_send->dst = data_rep->receiver;
            if (data_req->type == DATA_READ_REQ) {
                msg_to_send->flit_count = get_flit_count(1 + data_rep->word_count * 4);
            } else {
                msg_to_send->flit_count = get_flit_count(1);
            }

            msg_to_send->content = data_rep;
            entry->net_msg_to_send = msg_to_send;

            m_to_network_schedule_q[MSG_DATA_REP].push_back(msg_to_send);

            entry->status = _WORK_SEND_REMOTE_DATA_REP;
            ++it_addr;
            continue;

        } else if (entry->status == _WORK_SEND_REMOTE_DATA_REP) {
            if (data_rep->did_win_last_arbitration) {
                mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] sent a data rep for " << data_rep->maddr
                          << " to " << data_rep->receiver << endl;
                data_rep->did_win_last_arbitration = false;
                entry->net_msg_to_send = shared_ptr<message_t>();
                ++m_work_table_vacancy;
                m_work_table.erase(it_addr++);
                continue;
            } else {
                m_to_network_schedule_q[MSG_DATA_REP].push_back(msg_to_send);
                ++it_addr;
                continue;
            }
        }
    }

}

void sharedSharedEMRA::dram_work_table_update() {
    for (toDRAMTable::iterator it_addr = m_dram_work_table.begin(); it_addr != m_dram_work_table.end(); ) {
        shared_ptr<toDRAMEntry> entry = it_addr->second;
        shared_ptr<breakdownInfo> breakdown = static_pointer_cast<breakdownInfo>(entry->dram_req->breakdown);

        /* only reads are in the table */
        if (entry->dram_req->req->status() == DRAM_REQ_DONE) {

            if (!entry->dram_rep) {
                /* cost breakdown study */
                breakdown->dram_offchip = system_time - entry->milestone_time;

                entry->dram_rep = shared_ptr<dramMsg>(new dramMsg);
                entry->dram_rep->sender = m_id;
                entry->dram_rep->req = entry->dram_req->req;
                entry->dram_rep->did_win_last_arbitration = false;
                
                /* cost breakdown study */
                entry->dram_rep->milestone_time = system_time;

            }

            if (entry->dram_req->sender == m_id) {
                /* guaranteed to have an active entry in l2 work table */
                maddr_t start_maddr = entry->dram_req->req->maddr();
                mh_assert(m_work_table.count(start_maddr));
                m_work_table[start_maddr]->dram_rep = entry->dram_rep;
                mh_log(4) << "[DRAM " << m_id << " @ " << system_time << " ] has sent a DRAM rep for address " 
                          << entry->dram_rep->req->maddr() << " to core " << m_id << endl;
                m_dram_work_table.erase(it_addr++);
                continue;
            } else {
                if (!entry->dram_rep->did_win_last_arbitration) {
                    if (!entry->net_msg_to_send) {
                        shared_ptr<message_t> new_msg(new message_t);
                        new_msg->type = MSG_DRAM_REP;
                        new_msg->src = m_id;
                        new_msg->dst = entry->dram_req->sender;
                        uint32_t data_size = m_cfg.words_per_cache_line * 4;
                        new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                        new_msg->content = entry->dram_rep;
                        entry->net_msg_to_send = new_msg;
                    }
                    m_to_network_schedule_q[MSG_DRAM_REP].push_back(entry->net_msg_to_send);
                } else {
                    entry->dram_rep->did_win_last_arbitration = false;
                    if (entry->net_msg_to_send) {
                        entry->net_msg_to_send = shared_ptr<message_t>();
                    }
                    mh_log(4) << "[DRAM " << m_id << " @ " << system_time << " ] has sent a DRAM rep for address " 
                              << entry->dram_rep->req->maddr() << " to core " << entry->dram_req->sender << endl;
                    m_dram_work_table.erase(it_addr++);
                    continue;
                }
            }
        }
        ++it_addr;
    }
}

void sharedSharedEMRA::accept_incoming_messages() {

    while (m_core_receive_queues[MSG_DATA_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DATA_REP]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        maddr_t msg_start_maddr = get_start_maddr_in_line(data_msg->maddr);
        mh_assert(m_work_table.count(msg_start_maddr) && 
                  (m_work_table[msg_start_maddr]->status == _WORK_WAIT_REMOTE_DATA_REP));
        m_work_table[msg_start_maddr]->data_rep = data_msg;
        m_core_receive_queues[MSG_DATA_REP]->pop();
        break;
    }
    
    while (m_core_receive_queues[MSG_DATA_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DATA_REQ]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);

        if (data_msg->did_win_last_arbitration) {
            /* reset the flag for the next arbitration */
            data_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_DATA_REQ]->pop();
            /* this pop is supposed to be done in the previous cycle. continue to the next cycle */
            continue;
        } else {
            m_req_schedule_q.push_back(make_tuple(false, data_msg));
            break;
        }
    }

    while (m_core_receive_queues[MSG_DRAM_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAM_REQ]->front();
        shared_ptr<dramMsg> dram_msg = static_pointer_cast<dramMsg>(msg->content);
        if (dram_msg->did_win_last_arbitration) {
            dram_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_DRAM_REQ]->pop();
#if 0
            if (dram_msg->req->maddr().address == 0xefff46aae0 ||
                dram_msg->req->maddr().address == 0xefffbaaae0 ) 
            {
                cout << " DRAM REQ POPPED " << dram_msg->req->maddr() << " " << hex << dram_msg << dec << " @ " << system_time << " size " << m_core_receive_queues[MSG_DRAM_REQ]->size() << endl;
                if (m_core_receive_queues[MSG_DRAM_REQ]->size()) {
                    shared_ptr<message_t> nmsg = m_core_receive_queues[MSG_DRAM_REQ]->front();
                    shared_ptr<dramMsg> ndram_msg = static_pointer_cast<dramMsg>(nmsg->content);
                    cout << " DRAM REQ NEXT " << ndram_msg->req->maddr() << " " << hex << ndram_msg << dec << endl;
                }
            }
#endif
            continue;
        } else {
#if 0
            if (dram_msg->req->maddr().address == 0xefff46aae0 ||
                dram_msg->req->maddr().address == 0xefffbaaae0 ) 
            {
                cout << " DRAM REQ WAITING " << dram_msg->req->maddr() << " " << hex << dram_msg << dec << " @ " << system_time << " size " << m_core_receive_queues[MSG_DRAM_REQ]->size() << endl;
                if (m_core_receive_queues[MSG_DRAM_REQ]->size() > 2) {
                    shared_ptr<message_t> nmsg = m_core_receive_queues[MSG_DRAM_REQ]->at(1);
                    shared_ptr<dramMsg> ndram_msg = static_pointer_cast<dramMsg>(nmsg->content);
                    cout << " DRAM REQ NEXT " << ndram_msg->req->maddr() << " " << hex << ndram_msg << dec << endl;
                }
            }
#endif
            m_to_dram_req_schedule_q.push_back(dram_msg);
            break;
        }
    }

    if (m_core_receive_queues[MSG_DRAM_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAM_REP]->front();
        shared_ptr<dramMsg> dram_msg = static_pointer_cast<dramMsg>(msg->content);
        maddr_t start_maddr = dram_msg->req->maddr(); /* always access by a cache line */
        /* always for a read */
#if 0
        if (m_work_table.count(start_maddr) == 0) {
            cout << "[MEM " << m_id << " @ " << system_time << " ] received a DRAM reply for " << start_maddr << " but has no entry for it" << endl;
            cerr << "!!!!!!!!!!!!!!!!!!!!!!" << endl;
            while(true) {
                ;
            }
        }
#endif
        mh_assert(m_work_table.count(start_maddr) > 0);
        m_work_table[start_maddr]->dram_rep = dram_msg;
        m_core_receive_queues[MSG_DRAM_REP]->pop();
    }

}

void sharedSharedEMRA::schedule_requests() {

    /* random arbitration */
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    random_shuffle(m_core_port_schedule_q.begin(), m_core_port_schedule_q.end(), rr_fn);
    uint32_t count = 0;
    while (m_core_port_schedule_q.size()) {
        if (count < m_available_core_ports) {
            m_req_schedule_q.push_back(make_tuple(true, m_core_port_schedule_q.front()));
            ++count;
        } else {
            set_req_status(m_core_port_schedule_q.front(), REQ_RETRY);

            if (stats_enabled()) {
                stats()->add_memory_subsystem_serialization_cost(system_time - m_core_port_schedule_q.front()->milestone_time());
            }
        }
        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
    }

    random_shuffle(m_req_schedule_q.begin(), m_req_schedule_q.end(), rr_fn);
    while (m_req_schedule_q.size()) {
        bool is_core_req = m_req_schedule_q.front().get<0>();
        if (is_core_req) {
            shared_ptr<memoryRequest> req = static_pointer_cast<memoryRequest>(m_req_schedule_q.front().get<1>());
            maddr_t start_maddr = get_start_maddr_in_line(req->maddr());
            if (m_work_table.count(start_maddr) || m_available_core_ports == 0 || m_work_table_vacancy == 0) {

                set_req_status(req, REQ_RETRY);

                if (stats_enabled()) {
                    stats()->add_memory_subsystem_serialization_cost(system_time - req->milestone_time());
                }

                m_req_schedule_q.erase(m_req_schedule_q.begin());
                continue;
            }

            shared_ptr<tableEntry> new_entry(new tableEntry);

            new_entry->core_req = req;
            new_entry->status = (req->is_read())? _WORK_WAIT_CAT_AND_L1_FOR_LOCAL_READ : _WORK_WAIT_CAT_AND_L1_FOR_LOCAL_WRITE;

            new_entry->breakdown = shared_ptr<breakdownInfo>(new breakdownInfo());
            new_entry->breakdown->mem_serialization = system_time - req->milestone_time();
            new_entry->breakdown->cat_serialization = 0;
            new_entry->breakdown->cat_action = 0;
            new_entry->breakdown->l1_serialization = 0;
            new_entry->breakdown->l1_action = 0;
            new_entry->breakdown->ra_req_network_plus_serialization = 0;
            new_entry->breakdown->ra_rep_network_plus_serialization = 0;
            new_entry->breakdown->l2_serialization = 0;
            new_entry->breakdown->l2_action = 0;
            new_entry->breakdown->dram_req_onchip_network_plus_serialization = 0;
            new_entry->breakdown->dram_rep_onchip_network_plus_serialization = 0;
            new_entry->breakdown->dram_offchip = 0;
            new_entry->breakdown->temp_cat_serialization = 0;
            new_entry->breakdown->temp_cat_action = 0;
            new_entry->breakdown->temp_l1_serialization = 0;
            new_entry->breakdown->temp_l1_action = 0;
            new_entry->breakdown->temp_ra_req_network_plus_serialization = 0;
            new_entry->breakdown->temp_ra_rep_network_plus_serialization = 0;
            new_entry->breakdown->temp_l2_serialization = 0;
            new_entry->breakdown->temp_l2_action = 0;
            new_entry->breakdown->temp_dram_req_onchip_network_plus_serialization = 0;
            new_entry->breakdown->temp_dram_rep_onchip_network_plus_serialization = 0;
            new_entry->breakdown->temp_dram_offchip = 0;

            shared_ptr<cacheRequest> l1_req (new cacheRequest(req->maddr(),
                                                              req->is_read()? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                                              req->word_count(),
                                                              req->is_read()? shared_array<uint32_t>() : req->data()));
            l1_req->set_milestone_time(system_time);
            l1_req->set_reserve(false);
            l1_req->set_evict(false);
            l1_req->set_stats_info(new_entry->breakdown);

            new_entry->l1_req = l1_req;

            shared_ptr<catRequest> cat_req(new catRequest(req->maddr(), m_id));
            cat_req->set_milestone_time(system_time);
            cat_req->set_stats_info(new_entry->breakdown);
            new_entry->cat_req = cat_req;

            new_entry->data_req = shared_ptr<coherenceMsg>();
            new_entry->l1_evict_req = shared_ptr<cacheRequest>();
            new_entry->l2_req = shared_ptr<cacheRequest>();
            new_entry->data_rep = shared_ptr<coherenceMsg>();
            new_entry->dram_req = shared_ptr<dramMsg>();
            new_entry->dram_rep = shared_ptr<dramMsg>();

            new_entry->net_msg_to_send = shared_ptr<message_t>();

            new_entry->requested_time = system_time;

            new_entry->milestone_time = UINT64_MAX;
            set_req_status(req, REQ_WAIT);

            --m_available_core_ports;
            m_work_table[start_maddr] = new_entry;
            --m_work_table_vacancy;

            m_req_schedule_q.erase(m_req_schedule_q.begin());

            mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                      << " ] A core request on " << req->maddr() << " got into the table " << endl;

        } else {
            /* a request from a remote core */
            shared_ptr<coherenceMsg> req = static_pointer_cast<coherenceMsg>(m_req_schedule_q.front().get<1>());
            maddr_t start_maddr = get_start_maddr_in_line(req->maddr);
            if (m_work_table.count(start_maddr)) {
                //mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] A remote request on " 
                //          << start_maddr << " could not get into the table for an existing entry. " << endl;
                m_req_schedule_q.erase(m_req_schedule_q.begin());
            } else if (m_work_table_vacancy == 0) {
                //mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] A remote request on " 
                //          << start_maddr << " could not get into the table for the table is full. " << endl;
                m_req_schedule_q.erase(m_req_schedule_q.begin());
            } else {
                bool is_read = req->type == DATA_READ_REQ;

                req->did_win_last_arbitration = true;

                shared_ptr<tableEntry> new_entry(new tableEntry);

                new_entry->data_req = req;
                new_entry->status = 
                    (is_read)? _WORK_WAIT_L2_FOR_REMOTE_READ : _WORK_WAIT_L1_AND_L2_FOR_REMOTE_WRITE;

                new_entry->breakdown = req->breakdown;
                new_entry->breakdown->ra_req_network_plus_serialization += system_time - req->milestone_time;

                shared_ptr<cacheRequest> l2_req (new cacheRequest(req->maddr,
                                                                  (is_read)? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                                                  req->word_count,
                                                                  (is_read)? shared_array<uint32_t>() : req->data));
                l2_req->set_milestone_time(system_time);
                l2_req->set_reserve(true);
                l2_req->set_evict(true);
                l2_req->set_stats_info(new_entry->breakdown);

                new_entry->l2_req = l2_req;

                if (is_read) {
                    new_entry->l1_req = shared_ptr<cacheRequest>();
                } else {
                    shared_ptr<cacheRequest> l1_req (new cacheRequest(req->maddr,
                                                                      CACHE_REQ_WRITE,
                                                                      req->word_count,
                                                                      req->data));
                    l1_req->set_milestone_time(system_time);
                    l1_req->set_reserve(false);
                    l1_req->set_evict(false);
                    l1_req->set_stats_info(new_entry->breakdown);

                    new_entry->l1_req = l1_req;
                }

                new_entry->core_req = shared_ptr<memoryRequest>();
                new_entry->cat_req = shared_ptr<catRequest>();
                new_entry->l1_evict_req = shared_ptr<cacheRequest>();
                new_entry->data_rep = shared_ptr<coherenceMsg>();
                new_entry->dram_req = shared_ptr<dramMsg>();
                new_entry->dram_rep = shared_ptr<dramMsg>();

                new_entry->net_msg_to_send = shared_ptr<message_t>();

                new_entry->requested_time = UINT64_MAX;
                new_entry->milestone_time = UINT64_MAX;

                m_work_table[start_maddr] = new_entry;
                --m_work_table_vacancy;

                m_req_schedule_q.erase(m_req_schedule_q.begin());

                mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                          << " ] A remote request from " << req->sender << " on " << req->maddr << " got into the table " << endl;

            }
        }
    }
    m_req_schedule_q.clear();

    /* 4 : arbitrate inputs to dram work table */
    random_shuffle(m_to_dram_writeback_req_schedule_q.begin(), m_to_dram_writeback_req_schedule_q.end(), rr_fn);
    while (m_to_dram_writeback_req_schedule_q.size()) {
        mh_assert(m_dram_controller);
        shared_ptr<dramMsg> msg = m_to_dram_writeback_req_schedule_q.front();
        shared_ptr<breakdownInfo> breakdown = msg->breakdown;
        if (m_dram_controller->available()) {
            if (msg->req->is_read()) {

                breakdown->dram_req_onchip_network_plus_serialization = system_time - msg->milestone_time;

                mh_assert(!m_dram_work_table.count(msg->req->maddr()));

                shared_ptr<toDRAMEntry> new_entry(new toDRAMEntry);
                new_entry->dram_req = msg;
                new_entry->dram_rep = shared_ptr<dramMsg>();
                new_entry->net_msg_to_send = shared_ptr<message_t>();
                new_entry->breakdown = breakdown;

                /* cost breakdown study */
                new_entry->milestone_time = system_time;

                m_dram_work_table[msg->req->maddr()] = new_entry;
            }
            /* if write, make a request and done */
            m_dram_controller->request(msg->req);
            msg->did_win_last_arbitration = true;
        }
        m_to_dram_writeback_req_schedule_q.erase(m_to_dram_writeback_req_schedule_q.begin());
    }

    random_shuffle(m_to_dram_req_schedule_q.begin(), m_to_dram_req_schedule_q.end(), rr_fn);
    while (m_to_dram_req_schedule_q.size()) {
        mh_assert(m_dram_controller);
        shared_ptr<dramMsg> msg = m_to_dram_req_schedule_q.front();
        shared_ptr<breakdownInfo> breakdown = msg->breakdown;
        if (m_dram_controller->available() && !m_dram_work_table.count(msg->req->maddr())) {

#if 0
            if (msg->req->maddr().address == 0xefff46aae0 ||
                msg->req->maddr().address == 0xefffbaaae0 ) 
            {
                cout << " RECEIVED DRAM REQ " << msg->req->maddr() << " @ " << system_time << endl;
            }
#endif

            if (msg->req->is_read()) {

                breakdown->dram_req_onchip_network_plus_serialization = system_time - msg->milestone_time;

                shared_ptr<toDRAMEntry> new_entry(new toDRAMEntry);
                new_entry->dram_req = msg;
                new_entry->dram_rep = shared_ptr<dramMsg>();
                new_entry->net_msg_to_send = shared_ptr<message_t>();
                new_entry->breakdown = breakdown;

                /* cost breakdown study */
                new_entry->milestone_time = system_time;

                m_dram_work_table[msg->req->maddr()] = new_entry;
            }
            /* if write, make a request and done */
            m_dram_controller->request(msg->req);
            msg->did_win_last_arbitration = true;
        }
        m_to_dram_req_schedule_q.erase(m_to_dram_req_schedule_q.begin());
    }

    set<maddr_t> l1_requested_start_maddr;

    /* try make cache requests */
    for (workTable::iterator it_addr = m_work_table.begin(); it_addr != m_work_table.end(); ++it_addr) {
        maddr_t start_maddr = it_addr->first;
        shared_ptr<tableEntry> entry = it_addr->second;
        if (entry->l1_req && entry->l1_req->status() == CACHE_REQ_NEW) {
            /* not requested yet */
            shared_ptr<cacheRequest> l1_req = entry->l1_req;
            l1_requested_start_maddr.insert(get_start_maddr_in_line(l1_req->maddr()));
            if (l1_req->request_type() == CACHE_REQ_WRITE || 
                l1_req->request_type() == CACHE_REQ_UPDATE) {
                m_l1_write_req_schedule_q.push_back(l1_req);
            } else {
                m_l1_read_req_schedule_q.push_back(l1_req);
            }
        }
        if (entry->l2_req && entry->l2_req->status() == CACHE_REQ_NEW) {
            shared_ptr<cacheRequest> l2_req = entry->l2_req;
            if (l2_req->request_type() == CACHE_REQ_WRITE || 
                l2_req->request_type() == CACHE_REQ_UPDATE) {
                m_l2_write_req_schedule_q.push_back(l2_req);
            } else {
                m_l2_read_req_schedule_q.push_back(l2_req);
            }
        }

        if (entry->cat_req && entry->cat_req->status() == CAT_REQ_NEW) {
            m_cat_req_schedule_q.push_back(entry->cat_req);
        }
    }
    for (workTable::iterator it_addr = m_work_table.begin(); it_addr != m_work_table.end(); ++it_addr) {
        maddr_t start_maddr = it_addr->first;
        shared_ptr<tableEntry> entry = it_addr->second;
        if (entry->l1_evict_req && 
            entry->l1_evict_req->status() == CACHE_REQ_NEW &&
            l1_requested_start_maddr.count(entry->l1_evict_req->maddr()) == 0)
        {
            shared_ptr<cacheRequest> l1_evict_req = entry->l1_evict_req;
            m_l1_read_req_schedule_q.push_back(l1_evict_req);
        }
    }

    /* cat requests */
    random_shuffle(m_cat_req_schedule_q.begin(), m_cat_req_schedule_q.end(), rr_fn);
    while (m_cat->available() && m_cat_req_schedule_q.size()) {
        shared_ptr<catRequest> cat_req = m_cat_req_schedule_q.front();
        m_cat->request(cat_req);

        /* cost breakdown study */
        if (cat_req->milestone_time() != UINT64_MAX) {
            shared_ptr<breakdownInfo> breakdown =
                static_pointer_cast<breakdownInfo>(cat_req->stats_info());
            breakdown->temp_cat_serialization += system_time - cat_req->milestone_time();
            cat_req->set_milestone_time(system_time);
        }
        m_cat_req_schedule_q.erase(m_cat_req_schedule_q.begin());
    }
    m_cat_req_schedule_q.clear();

    /* l1 read requests */
    random_shuffle(m_l1_read_req_schedule_q.begin(), m_l1_read_req_schedule_q.end(), rr_fn);
    while (m_l1->read_port_available() && m_l1_read_req_schedule_q.size()) {
        shared_ptr<cacheRequest> l1_req = m_l1_read_req_schedule_q.front();
        m_l1->request(l1_req);

        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        if (l1_req->milestone_time() != UINT64_MAX) {
            shared_ptr<breakdownInfo> breakdown = static_pointer_cast<breakdownInfo>(l1_req->stats_info());
            breakdown->temp_l1_serialization += system_time - l1_req->milestone_time();
            l1_req->set_milestone_time(system_time);
        }
        m_l1_read_req_schedule_q.erase(m_l1_read_req_schedule_q.begin());
    }
    m_l1_read_req_schedule_q.clear();
    
    /* l1 write requests */
    random_shuffle(m_l1_write_req_schedule_q.begin(), m_l1_write_req_schedule_q.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q.size()) {
        shared_ptr<cacheRequest> l1_req = m_l1_write_req_schedule_q.front();
        m_l1->request(l1_req);

        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        if (l1_req->milestone_time() != UINT64_MAX) {
            shared_ptr<breakdownInfo> breakdown = static_pointer_cast<breakdownInfo>(l1_req->stats_info());
            breakdown->temp_l1_serialization += system_time - l1_req->milestone_time();
            l1_req->set_milestone_time(system_time);
        }
        m_l1_write_req_schedule_q.erase(m_l1_write_req_schedule_q.begin());
    }
    m_l1_write_req_schedule_q.clear();
    
    /* l2 read requests */
    random_shuffle(m_l2_read_req_schedule_q.begin(), m_l2_read_req_schedule_q.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q.size()) {
        shared_ptr<cacheRequest> l2_req = m_l2_read_req_schedule_q.front();
        m_l2->request(l2_req);

        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        if (l2_req->milestone_time() != UINT64_MAX) {
            shared_ptr<breakdownInfo> breakdown = static_pointer_cast<breakdownInfo>(l2_req->stats_info());
            breakdown->temp_l2_serialization += system_time - l2_req->milestone_time();
            l2_req->set_milestone_time(system_time);
        }
        m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
    }
    m_l2_read_req_schedule_q.clear();
    
    /* l2 write requests */
    random_shuffle(m_l2_write_req_schedule_q.begin(), m_l2_write_req_schedule_q.end(), rr_fn);
    while (m_l2->write_port_available() && m_l2_write_req_schedule_q.size()) {
        shared_ptr<cacheRequest> l2_req = m_l2_write_req_schedule_q.front();
        m_l2->request(l2_req);

        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        if (l2_req->milestone_time() != UINT64_MAX) {
            shared_ptr<breakdownInfo> breakdown = static_pointer_cast<breakdownInfo>(l2_req->stats_info());
            breakdown->temp_l2_serialization += system_time - l2_req->milestone_time();
            l2_req->set_milestone_time(system_time);
        }
        m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
    }
    m_l2_write_req_schedule_q.clear();

    /* networks */
    for (uint32_t it_channel = 0; it_channel < NUM_MSG_TYPES; ++it_channel) {
        random_shuffle(m_to_network_schedule_q_priority[it_channel].begin(), m_to_network_schedule_q_priority[it_channel].end(), rr_fn);
        while (m_to_network_schedule_q_priority[it_channel].size()) {
            shared_ptr<message_t> msg = m_to_network_schedule_q_priority[it_channel].front();
            if (m_core_send_queues[it_channel]->push_back(msg)) {
                mh_log(4) << "[NETpriority " << m_id << " @ " << system_time << " ] network msg gone " 
                          << m_id << " -> " << msg->dst << " type " << it_channel << " num flits " << msg->flit_count << endl;
                switch (it_channel) {
                case MSG_DATA_REQ:
                case MSG_DATA_REP:
                    {
                        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
                        data_msg->did_win_last_arbitration = true;
                        break;
                    }
                case MSG_DRAM_REQ:
                case MSG_DRAM_REP:
                    {
                        shared_ptr<dramMsg> dram_msg = static_pointer_cast<dramMsg>(msg->content);
                        dram_msg->did_win_last_arbitration = true;
                        break;
                    }
                default:
                    mh_assert(false);
                    break;
                }
            } else {
                break;
            }
            m_to_network_schedule_q_priority[it_channel].erase(m_to_network_schedule_q_priority[it_channel].begin());
        }
    }
    m_to_network_schedule_q_priority.clear();


    for (uint32_t it_channel = 0; it_channel < NUM_MSG_TYPES; ++it_channel) {
        random_shuffle(m_to_network_schedule_q[it_channel].begin(), m_to_network_schedule_q[it_channel].end(), rr_fn);
        while (m_to_network_schedule_q[it_channel].size()) {
            shared_ptr<message_t> msg = m_to_network_schedule_q[it_channel].front();
            if (m_core_send_queues[it_channel]->push_back(msg)) {
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] network msg gone " 
                          << m_id << " -> " << msg->dst << " type " << it_channel << " num flits " << msg->flit_count << endl;
                switch (it_channel) {
                case MSG_DATA_REQ:
                case MSG_DATA_REP:
                    {
                        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
                        data_msg->did_win_last_arbitration = true;
                        break;
                    }
                case MSG_DRAM_REQ:
                case MSG_DRAM_REP:
                    {
                        shared_ptr<dramMsg> dram_msg = static_pointer_cast<dramMsg>(msg->content);
                        dram_msg->did_win_last_arbitration = true;
                        break;
                    }
                default:
                    mh_assert(false);
                    break;
                }
            } else {
                break;
            }
            m_to_network_schedule_q[it_channel].erase(m_to_network_schedule_q[it_channel].begin());
        }
    }
    m_to_network_schedule_q.clear();

}

