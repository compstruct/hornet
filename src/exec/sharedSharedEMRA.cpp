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

/* Used for tentative stats per memory instruction (outstanding costs survive) */
#define T_IDX_CAT 0
#define T_IDX_L1 1
#define T_IDX_L2 2
#define T_IDX_FEED_L1 3
#define T_IDX_FEED_L2 4

sharedSharedEMRA::sharedSharedEMRA(uint32_t id, 
                                   const uint64_t &t, 
                                   std::shared_ptr<tile_statistics> st, 
                                   logger &l, 
                                   std::shared_ptr<random_gen> r, 
                                   std::shared_ptr<cat> a_cat, 
                                   sharedSharedEMRACfg_t cfg) :
    memory(id, t, st, l, r), 
    m_cfg(cfg), 
    m_l1(NULL), 
    m_l2(NULL), 
    m_cat(a_cat), 
    m_stats(std::shared_ptr<sharedSharedEMRAStatsPerTile>()),
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

    m_cat_req_schedule_q.reserve(m_cfg.work_table_size);
    m_l1_read_req_schedule_q.reserve(m_cfg.work_table_size);
    m_l1_write_req_schedule_q.reserve(m_cfg.work_table_size);
    m_l2_read_req_schedule_q.reserve(m_cfg.work_table_size);
    m_l2_write_req_schedule_q.reserve(m_cfg.work_table_size);
    m_ra_req_schedule_q.reserve(m_cfg.work_table_size);
    m_ra_rep_schedule_q.reserve(m_cfg.work_table_size);
    m_dramctrl_req_schedule_q.reserve(m_cfg.work_table_size + 1);
    m_dramctrl_rep_schedule_q.reserve(m_cfg.work_table_size);

}

sharedSharedEMRA::~sharedSharedEMRA() {
    delete m_l1;
    delete m_l2;
}

uint32_t sharedSharedEMRA::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

void sharedSharedEMRA::request(std::shared_ptr<memoryRequest> req) {

    /* assumes a request is not across multiple cache lines */
    uint32_t __attribute__((unused)) byte_offset = req->maddr().address%(m_cfg.words_per_cache_line*4);
    mh_assert( (byte_offset + req->word_count()*4) <= m_cfg.words_per_cache_line * 4);

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    /* per memory instruction info */
    if (req->per_mem_instr_runtime_info()) {
            std::shared_ptr<std::shared_ptr<void> > p_runtime_info = req->per_mem_instr_runtime_info();
            std::shared_ptr<void>& runtime_info = *p_runtime_info;
            std::shared_ptr<sharedSharedEMRAStatsPerMemInstr> per_mem_instr_stats;
        if (!runtime_info) {
            /* no per-instr stats: this is the first time this memory instruction is issued */
            per_mem_instr_stats = std::shared_ptr<sharedSharedEMRAStatsPerMemInstr>(new sharedSharedEMRAStatsPerMemInstr(req->is_read()));
            per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
            runtime_info = per_mem_instr_stats;
        } else {
            per_mem_instr_stats = 
                static_pointer_cast<sharedSharedEMRAStatsPerMemInstr>(*req->per_mem_instr_runtime_info());
            if (per_mem_instr_stats->is_in_migration()) {
                per_mem_instr_stats = static_pointer_cast<sharedSharedEMRAStatsPerMemInstr>(runtime_info);
                per_mem_instr_stats->migration_finished(system_time, stats_enabled());
                per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
            }
        }
    }

    /* will schedule for a core port and a work table entry in schedule function */
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
    if(m_dramctrl) {
        m_dramctrl->tick_positive_edge();
    }
}

void sharedSharedEMRA::tick_negative_edge() {

    m_l1->tick_negative_edge();
    m_l2->tick_negative_edge();
    m_cat->tick_negative_edge();
    if(m_dramctrl) {
        m_dramctrl->tick_negative_edge();
    }

    /* accept messages and write into tables */
    accept_incoming_messages();

    update_work_table();

    update_dramctrl_work_table();

}

void sharedSharedEMRA::update_work_table() {

    for (workTable::iterator it_addr = m_work_table.begin(); it_addr != m_work_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        std::shared_ptr<tableEntry>& entry = it_addr->second;

#if 0
        if (system_time > 120000) {
            mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] in state " << entry->status 
                << " for " << start_maddr << endl;
        }
#endif

        std::shared_ptr<memoryRequest>& core_req = entry->core_req;
        std::shared_ptr<catRequest>& cat_req = entry->cat_req;
        std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;
        std::shared_ptr<cacheRequest>& l2_req = entry->l2_req;
        std::shared_ptr<coherenceMsg>& ra_req = entry->ra_req;;
        std::shared_ptr<coherenceMsg>& ra_rep = entry->ra_rep; 
        std::shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
        std::shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;

        std::shared_ptr<cacheLine> l1_line = (l1_req)? l1_req->line_copy() : std::shared_ptr<cacheLine>();
        std::shared_ptr<cacheLine> l2_line = (l2_req)? l2_req->line_copy() : std::shared_ptr<cacheLine>();
        std::shared_ptr<cacheLine> l1_victim = (l1_req)? l1_req->line_to_evict_copy() : std::shared_ptr<cacheLine>();
        std::shared_ptr<cacheLine> l2_victim = (l2_req)? l2_req->line_to_evict_copy() : std::shared_ptr<cacheLine>();

        std::shared_ptr<sharedSharedEMRAStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        if (entry->status == _CAT_AND_L1_FOR_LOCAL) {

            if (l1_req->status() == CACHE_REQ_HIT) {

                /* it's for sure that it get's a core hit */
                cat_req = std::shared_ptr<catRequest>();

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l1_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);
                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 HIT and finish serving address "
                          << core_req->maddr() << endl;

                if (stats_enabled()) {
                    /* sharedSharedEMRAStatsPerMemInstr */
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                        per_mem_instr_stats->add_local_l1_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                        /* sharedSharedEMRAStatsPerTile */
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }

                    if (core_req->is_read()) {
                        stats()->new_read_instr_at_l1();
                        stats()->hit_for_read_instr_at_l1();
                        stats()->hit_for_read_instr_at_local_l1();
                        stats()->did_finish_read();
                    } else {
                        stats()->new_write_instr_at_l1();
                        stats()->hit_for_write_instr_at_l1();
                        stats()->hit_for_write_instr_at_local_l1();
                        stats()->did_finish_write();
                    }
                } else {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->clear_tentative_data();
                    }
                }

                ++m_work_table_vacancy;
                m_work_table.erase(it_addr++);
                continue;
                /* FINISHED */

            } else {

                /* record when CAT/L1 is finished */
                if (cat_req->operation_begin_time() != UINT64_MAX && cat_req->status() == CAT_REQ_DONE) {
                    if (stats_enabled() && per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_CAT)->add_cat_ops(system_time - cat_req->operation_begin_time());
                    }
                    cat_req->set_operation_begin_time(UINT64_MAX);
                }

                if (l1_req->operation_begin_time() != UINT64_MAX && l1_req->status() == CACHE_REQ_MISS) {
                    if (stats_enabled() && per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                    }
                    l1_req->set_operation_begin_time(UINT64_MAX);
                }

                if (cat_req->status() == CAT_REQ_DONE) {
                    /* when L1 is yet to finish, cannot continue without CAT info */
                    uint32_t home = cat_req->home();
                    if (home != m_id) {
                        /* L1 must miss */
                        l1_req = std::shared_ptr<cacheRequest>();

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a core miss for "
                                  << core_req->maddr() << endl;

                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->core_missed();
                        }
                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                per_mem_instr_stats->commit_tentative_data(T_IDX_CAT);
                            }
                            if (core_req->is_read()) {
                                stats()->new_read_instr_at_l1();
                                stats()->core_miss_for_read_instr_at_l1();
                            } else {
                                stats()->new_write_instr_at_l1();
                                stats()->core_miss_for_write_instr_at_l1();
                            }
                        } else if (per_mem_instr_stats) {
                            per_mem_instr_stats->clear_tentative_data();
                        }

                        if (m_cfg.logic == MIGRATION_ALWAYS) {
                            if (per_mem_instr_stats) {
                                per_mem_instr_stats->migration_started(system_time);
                            }
                            set_req_status(core_req, REQ_MIGRATE);
                            set_req_home(core_req, home);
                            ++m_available_core_ports;
                            ++m_work_table_vacancy;
                            m_work_table.erase(it_addr++);
                            continue;
                            /* FINISHED */
                        }

                        ra_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                        ra_req->sender = m_id;
                        ra_req->receiver = home;
                        if (core_req->is_read()) {
                            ra_req->type = RA_READ_REQ;
                            ra_req->data = shared_array<uint32_t>();
                        } else {
                            ra_req->type = RA_WRITE_REQ;
                            ra_req->data = core_req->data();
                        }
                        ra_req->word_count = core_req->word_count();
                        ra_req->maddr = core_req->maddr();
                        ra_req->sent = false;
                        ra_req->birthtime = system_time;
                        ra_req->per_mem_instr_stats = per_mem_instr_stats;

                        m_ra_req_schedule_q.push_back(entry);

                        entry->status = _SEND_RA_REQ;

                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else if (l1_req->status() == CACHE_REQ_MISS) {
                        /* core hit, true L1 miss */

                        /* stats */
                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                /* apply the outstanding latency */
                                if (per_mem_instr_stats->get_max_tentative_data_index() == T_IDX_L1) {
                                    per_mem_instr_stats->add_local_l1_cost_for_miss
                                        (per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                                }
                                per_mem_instr_stats->commit_max_tentative_data();
                            }
                            if (core_req->is_read()) {
                                stats()->new_read_instr_at_l1();
                                stats()->true_miss_for_read_instr_at_l1();
                                stats()->true_miss_for_read_instr_at_local_l1();
                            } else {
                                stats()->new_write_instr_at_l1();
                                stats()->true_miss_for_write_instr_at_l1();
                                stats()->true_miss_for_write_instr_at_local_l1();
                            }
                        } else if (per_mem_instr_stats) {
                            per_mem_instr_stats->clear_tentative_data();
                        }

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a core hit but a true L1 MISS for "
                                  << core_req->maddr() << endl;

                        if (core_req->is_read()) {
                            l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_READ,
                                                                               m_cfg.words_per_cache_line));
                        } else {
                            l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(core_req->maddr(), CACHE_REQ_WRITE,
                                                                               core_req->word_count(), core_req->data()));
                        }
                        l2_req->set_serialization_begin_time(system_time);
                        l2_req->set_unset_dirty_on_write(false);
                        l2_req->set_claim(false);
                        l2_req->set_evict(false);

                        if (l2_req->use_read_ports()) {
                            m_l2_read_req_schedule_q.push_back(entry);
                        } else {
                            m_l2_write_req_schedule_q.push_back(entry);
                        }

                        entry->status = _L2;

                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    }
                }

                /* CAT not ready, or CAT hit and L1 not ready */

                /* make a request again if lost the last arbitration */
                if (cat_req->status() == CAT_REQ_NEW) {
                    m_cat_req_schedule_q.push_back(entry);
                } 
                if (l1_req->status() == CACHE_REQ_NEW) {
                    if (l1_req->use_read_ports()) {
                        m_l1_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l1_write_req_schedule_q.push_back(entry);
                    }
                }
                ++it_addr;
                continue;
                /* SPIN */

            }
            /* _CAT_AND_L1_FOR_LOCAL - never reach here */

        } else if (entry->status == _SEND_RA_REQ) {

            if (ra_req->sent) {
                entry->status = _WAIT_RA_REP;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent an RA request (" << ra_req->type
                          << ") to " << ra_req->receiver << " for " << ra_req->maddr << endl;
                ++it_addr;
                continue;
                /* TRANSITION */
            } else {
                m_ra_req_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }
            /* _SEND_RA_REQ - never reach here */

        } else if (entry->status == _WAIT_RA_REP) {

            if (ra_rep) {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a RA reply (" << ra_rep->type
                          << ") from " << ra_rep->sender << " for " << ra_rep->maddr << endl;

                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->add_ra_rep_nas(system_time - ra_rep->birthtime);
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                    if (core_req->is_read()) {
                        stats()->did_finish_read();
                    } else {
                        stats()->did_finish_write();
                    }
                }

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = ra_rep->data[i + word_offset];
                }
                set_req_data(core_req, ret);

                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;
                ++m_work_table_vacancy;

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finished serving address "
                          << core_req->maddr() << " by remote accesses" << endl;

                m_work_table.erase(it_addr++);
                continue;
                /* FINISH */
            } else {
                ++it_addr;
                continue;
                /* SPIN */
            }
            /* _WAIT_RA_REP - never reach here */

        } else if (entry->status == _L1_FOR_REMOTE) {

            if (l1_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->serialization_begin_time());
                    if (l1_req->status() == CACHE_REQ_HIT) {
                        per_mem_instr_stats->add_remote_l1_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    } else {
                        per_mem_instr_stats->add_remote_l1_cost_for_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l1_req->status() == CACHE_REQ_HIT) {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets an L1 HIT for a remote request on "
                          << ra_req->maddr << endl;

                if (stats_enabled()) {
                    if (ra_req->type == RA_READ_REQ) {
                        stats()->new_read_instr_at_l1();
                        stats()->hit_for_read_instr_at_l1();
                        stats()->hit_for_read_instr_at_remote_l1();
                    } else {
                        stats()->new_write_instr_at_l1();
                        stats()->hit_for_write_instr_at_l1();
                        stats()->hit_for_write_instr_at_remote_l1();
                    }
                }

                ra_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                ra_rep->sender = m_id;
                ra_rep->sent = false;
                ra_rep->type = RA_REP;
                ra_rep->receiver = ra_req->sender;
                if (ra_req->type == RA_READ_REQ) {
                    ra_rep->word_count = ra_req->word_count;
                } else {
                    ra_rep->word_count = 0;
                }
                ra_rep->maddr = ra_req->maddr;
                ra_rep->data = l1_line->data;
                ra_rep->birthtime = system_time;

                entry->status = _SEND_RA_REP;
                m_ra_rep_schedule_q.push_back(entry);

                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                /* miss */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets an L1 MISS for a remote request on "
                          << ra_req->maddr << endl;

                if (stats_enabled()) {
                    if (ra_req->type == RA_READ_REQ) {
                        stats()->new_read_instr_at_l1();
                        stats()->true_miss_for_read_instr_at_l1();
                        stats()->true_miss_for_read_instr_at_remote_l1();
                    } else {
                        stats()->new_write_instr_at_l1();
                        stats()->true_miss_for_write_instr_at_l1();
                        stats()->true_miss_for_write_instr_at_remote_l1();
                    }
                }

                if (ra_req->type == RA_READ_REQ) {
                    l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_READ,
                                                                       m_cfg.words_per_cache_line));
                } else {
                    l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(ra_req->maddr, CACHE_REQ_WRITE,
                                                                       ra_req->word_count, ra_req->data));
                }
                l2_req->set_serialization_begin_time(system_time);
                l2_req->set_unset_dirty_on_write(false);
                l2_req->set_claim(false);
                l2_req->set_evict(false);

                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }

                entry->status = _L2;

                ++it_addr;
                continue;
                /* TRANSITION */
            }
            /* _L1_FOR_REMOTE - never reach here */

        } else if (entry->status == _L2) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                    if (l2_req->status() == CACHE_REQ_HIT) {
                        if (core_req) {
                            per_mem_instr_stats->add_local_l2_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        } else {
                            per_mem_instr_stats->add_remote_l2_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        }
                    } else {
                        if (core_req) {
                            per_mem_instr_stats->add_local_l2_cost_for_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        } else {
                            per_mem_instr_stats->add_remote_l2_cost_for_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        }
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l2_req->status() == CACHE_REQ_HIT) {
                if (stats_enabled()) {
                    if ((core_req && core_req->is_read()) || (ra_req && ra_req->type == RA_READ_REQ)) {
                        stats()->new_read_instr_at_l2();
                        stats()->hit_for_read_instr_at_l2();
                        if (core_req) {
                            stats()->hit_for_read_instr_at_local_l2();
                        } else {
                            stats()->hit_for_read_instr_at_remote_l2();
                        }
                    } else {
                        stats()->new_write_instr_at_l2();
                        stats()->hit_for_write_instr_at_l2();
                        if (core_req) {
                            stats()->hit_for_write_instr_at_local_l2();
                        } else {
                            stats()->hit_for_write_instr_at_remote_l2();
                        }
                    }
                }

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 HIT for a request on "
                          << l2_req->maddr() << endl;

                l1_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line,
                                                                   l2_line->data));
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(true);
                l1_req->set_claim(true);
                l1_req->set_evict(true);

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }

                entry->status = _FILL_L1;

                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                /* miss */
                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 MISS for a request on "
                          << l2_req->maddr() << endl;

                if (stats_enabled()) {
                    if ((core_req && core_req->is_read()) || (ra_req && ra_req->type == RA_READ_REQ)) {
                        stats()->new_read_instr_at_l2();
                        stats()->true_miss_for_read_instr_at_l2();
                        if (core_req) {
                            stats()->true_miss_for_read_instr_at_local_l2();
                        } else {
                            stats()->true_miss_for_read_instr_at_remote_l2();
                        }
                    } else {
                        stats()->new_write_instr_at_l2();
                        stats()->true_miss_for_write_instr_at_l2();
                        if (core_req) {
                            stats()->true_miss_for_write_instr_at_local_l2();
                        } else {
                            stats()->true_miss_for_write_instr_at_remote_l2();
                        }
                    }
                }

                dramctrl_req = std::shared_ptr<dramctrlMsg>(new dramctrlMsg);
                dramctrl_req->sender = m_id;
                dramctrl_req->receiver = m_dramctrl_location;
                dramctrl_req->maddr = start_maddr;
                dramctrl_req->dram_req = std::shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                 DRAM_REQ_READ,
                                                                                 m_cfg.words_per_cache_line));
                dramctrl_req->sent = false;
                dramctrl_req->birthtime = system_time;
                dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark it as from local */

                entry->status = _SEND_DRAMCTRL_REQ;

                ++it_addr;
                continue;
                /* TRANSITION */
            }
            /* _L2 - never reach here */

        } else if (entry->status == _FILL_L1) {

            if (l1_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->serialization_begin_time());
                    if (ra_rep) {
                        per_mem_instr_stats->add_remote_l1_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    } else {
                        per_mem_instr_stats->add_local_l1_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l1_req->status() == CACHE_REQ_HIT) {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] is updated for the line of "
                          << l1_req->maddr() << endl;

                /* ready to make replies first */
                if (core_req) {
                    shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                    uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                    for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                        ret[i] = l1_line->data[i + word_offset];
                    }
                    set_req_data(core_req, ret);
                } else {
                    ra_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                    ra_rep->sender = m_id;
                    ra_rep->sent = false;
                    ra_rep->type = RA_REP;
                    ra_rep->receiver = ra_req->sender;
                    if (ra_req->type == RA_READ_REQ) {
                        ra_rep->word_count = ra_req->word_count;
                    } else {
                        ra_rep->word_count = 0;
                    }
                    ra_rep->maddr = ra_req->maddr;
                    ra_rep->data = l1_line->data;
                    ra_rep->birthtime = system_time;
                }

                if (l1_victim && stats_enabled()) {
                    stats()->evict_at_l1();
                }

                if (l1_victim && l1_victim->data_dirty) {
                    if (stats_enabled()) {
                        stats()->writeback_at_l1();
                    }

                    l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(l1_victim->start_maddr,
                                                                       CACHE_REQ_UPDATE,
                                                                       m_cfg.words_per_cache_line,
                                                                       l1_victim->data));
                    l2_req->set_serialization_begin_time(system_time);
                    l2_req->set_unset_dirty_on_write(false);
                    l2_req->set_claim(false);
                    l2_req->set_evict(false);

                    if (l2_req->use_read_ports()) {
                        m_l2_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l2_write_req_schedule_q.push_back(entry);
                    }
                    /* need to declare writebacking (for simulation performance) */
                    m_l2_writeback_status[l1_victim->start_maddr] = entry;
                    
                    entry->status = _WRITEBACK_L2;

                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else if (core_req) {

                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                            stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                        }
                        if (core_req->is_read()) {
                            stats()->did_finish_read();
                        } else {
                            stats()->did_finish_write();
                        }
                    }

                    set_req_status(core_req, REQ_DONE);
                    ++m_available_core_ports;
                    ++m_work_table_vacancy;
                    
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finished serving address "
                              << core_req->maddr() << " for the local core" << endl;

                    m_work_table.erase(it_addr++);
                    continue;
                    /* FINISH */

                } else {

                    /* requested by remote */
                    m_ra_rep_schedule_q.push_back(entry);
                    entry->status = _SEND_RA_REP;

                    ++it_addr;
                    continue;
                    /* TRANSITION */

                }
            } else {
                /* miss */
                /* lines are all reserved */
                l1_req->set_serialization_begin_time(system_time);
                l1_req->reset();

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }

                ++it_addr;
                continue;
                /* SPIN */
            }
            /* _FILL_L1 - never reach here */

        } else if (entry->status == _WRITEBACK_L2) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                /* need to declare writebacking (for simulation performance) */
                m_l2_writeback_status[l1_victim->start_maddr] = entry;
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                    if (core_req) {
                        per_mem_instr_stats->add_local_l2_cost_for_writeback(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    } else {
                        per_mem_instr_stats->add_remote_l2_cost_for_writeback(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l2_req->status() == CACHE_REQ_HIT) {
                /* a reply is already set in _FILL_L1 */
                if (core_req) {
                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                            stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                        }
                        if (core_req->is_read()) {
                            stats()->did_finish_read();
                        } else {
                            stats()->did_finish_write();
                        }
                    }

                    set_req_status(core_req, REQ_DONE);
                    ++m_available_core_ports;
                    ++m_work_table_vacancy;
                    
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finished serving address "
                              << core_req->maddr() << " for the local core" << endl;

                    m_work_table.erase(it_addr++);
                    continue;
                    /* FINISH */

                } else {
                    m_ra_rep_schedule_q.push_back(entry);
                    entry->status = _SEND_RA_REP;

                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }

            } else {
                /* miss - send writeback to dram */
                dramctrl_req = std::shared_ptr<dramctrlMsg>(new dramctrlMsg);
                dramctrl_req->sender = m_id;
                dramctrl_req->receiver = m_dramctrl_location;
                dramctrl_req->maddr = l1_victim->start_maddr;
                dramctrl_req->dram_req = std::shared_ptr<dramRequest>(new dramRequest(l1_victim->start_maddr,
                                                                                 DRAM_REQ_WRITE,
                                                                                 m_cfg.words_per_cache_line,
                                                                                 l1_victim->data));
                dramctrl_req->sent = false;
                dramctrl_req->birthtime = system_time;
                dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                entry->status = _WRITEBACK_DRAMCTRL_FOR_L1_FILL;

                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                m_dramctrl_writeback_status[l1_victim->start_maddr] = entry;

                ++it_addr;
                continue;
                /* TRANSITION */

            }
            /* _WRITEBACK_L2 - never reach here */

        } else if (entry->status == _WRITEBACK_DRAMCTRL_FOR_L1_FILL) {
            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                m_dramctrl_writeback_status[l1_victim->start_maddr] =  entry;
                ++it_addr;
                continue;
                /* SPIN */
            }

            /* a reply is already set in _FILL_L1 */
            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dramctrl_req_nas(system_time - dramctrl_req->birthtime);
            }

            if (core_req) {
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                    if (core_req->is_read()) {
                        stats()->did_finish_read();
                    } else {
                        stats()->did_finish_write();
                    }
                }

                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;
                ++m_work_table_vacancy;

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finished serving address "
                    << core_req->maddr() << " for the local core" << endl;

                m_work_table.erase(it_addr++);
                continue;
                /* FINISH */

            } else {
                m_ra_rep_schedule_q.push_back(entry);
                entry->status = _SEND_RA_REP;

                ++it_addr;
                continue;
                /* TRANSITION */
            }
            /* _WRITEBACK_DRAMCTRL_FOR_L1_FILL - never reach here */

        } else if (entry->status == _SEND_RA_REP) {
            if (!ra_rep->sent) {
                m_ra_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] sent an RA rep for " << ra_rep->maddr
                      << " to " << ra_rep->receiver << endl;
            ++m_work_table_vacancy;
            m_work_table.erase(it_addr++);
            continue;
            /* FINISH */

        } else if (entry->status == _WAIT_DRAMCTRL_REP) {

            if (!dramctrl_rep) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dramctrl_rep_nas(system_time - dramctrl_rep->birthtime);
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] received a DRAM reply for "
                      << dramctrl_rep->dram_req->maddr() << endl;

            if (l2_req->request_type() == CACHE_REQ_WRITE) {
                uint32_t word_offset = (l2_req->maddr().address / 4 )  % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < l2_req->word_count(); ++i) {
                    dramctrl_rep->dram_req->read()[i + word_offset] = l2_req->data_to_write()[i];
                }
            }

            l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line, 
                                                               dramctrl_rep->dram_req->read()));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(l2_req->request_type() == CACHE_REQ_READ);
            l2_req->set_claim(true);
            l2_req->set_evict(true);

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(entry);
            } else {
                m_l2_write_req_schedule_q.push_back(entry);
            }

            entry->status = _FILL_L2;

            ++it_addr;
            continue;
            /* TRANSITION */

        } else if (entry->status == _SEND_DRAMCTRL_REQ) {
            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry));
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a DRAM request for "
                      << dramctrl_req->dram_req->maddr() << " to " << dramctrl_req->receiver << endl;
            
            entry->status = _WAIT_DRAMCTRL_REP;
            ++it_addr;
            continue;
            /* TRANSITION */

        } else if (entry->status == _FILL_L2) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                /* lost last arbitration */
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                    if (core_req) {
                        per_mem_instr_stats->add_local_l2_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    } else {
                        per_mem_instr_stats->add_remote_l2_cost_for_update(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            if (l2_req->status() == CACHE_REQ_HIT) {

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] is updated for the line of "
                          << l2_req->maddr() << endl;

                /* ready to make l1 fill request first */

                l1_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line,
                                                                   l2_line->data));
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(true);
                l1_req->set_claim(true);
                l1_req->set_evict(true);

                if (l2_victim && stats_enabled()) {
                    stats()->evict_at_l2();
                }

                if (l2_victim && l2_victim->data_dirty) {
                    dramctrl_req = std::shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = l2_victim->start_maddr;
                    dramctrl_req->dram_req = std::shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                     DRAM_REQ_WRITE,
                                                                                     m_cfg.words_per_cache_line,
                                                                                     l2_victim->data));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    entry->status = _WRITEBACK_DRAMCTRL_FOR_L2_FILL;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                    m_dramctrl_writeback_status[l2_victim->start_maddr] = entry;

                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else {

                    if (l1_req->use_read_ports()) {
                        m_l1_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l1_write_req_schedule_q.push_back(entry);
                    }
                    entry->status = _FILL_L1;

                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }
            } else {
                /* miss */
                /* lines are all reserved */
                l2_req->set_serialization_begin_time(system_time);
                l2_req->reset();

                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }

                ++it_addr;
                continue;
                /* SPIN */
            }
            /* _FILL_L2 - never reach here */
        } else if (entry->status == _WRITEBACK_DRAMCTRL_FOR_L2_FILL) {

            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                m_dramctrl_writeback_status[l2_victim->start_maddr] =  entry;
                ++it_addr;
                continue;
                /* SPIN */
            }

            /* a l1 fill request is already set in _FILL_L2 */
            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dramctrl_req_nas(system_time - dramctrl_req->birthtime);
            }

            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(entry);
            } else {
                m_l1_write_req_schedule_q.push_back(entry);
            }
            entry->status = _FILL_L1;

            ++it_addr;
            continue;

            /* TRANSITION */

        }
    }

}

void sharedSharedEMRA::update_dramctrl_work_table() {

    for (dramctrlTable::iterator it_addr = m_dramctrl_work_table.begin(); it_addr != m_dramctrl_work_table.end(); ) {
            std::shared_ptr<dramctrlTableEntry>& entry = it_addr->second;
            std::shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
            std::shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;
            std::shared_ptr<sharedSharedEMRAStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        /* only reads are in the table */
        if (dramctrl_req->dram_req->status() == DRAM_REQ_DONE) {

            if (!dramctrl_rep) {
                if (stats_enabled() && per_mem_instr_stats) {
                    per_mem_instr_stats->add_dram_ops(system_time - entry->operation_begin_time);
                }
                dramctrl_rep = std::shared_ptr<dramctrlMsg>(new dramctrlMsg);
                dramctrl_rep->sender = m_id;
                dramctrl_rep->maddr = dramctrl_req->maddr;
                dramctrl_rep->dram_req = dramctrl_req->dram_req;
                dramctrl_rep->sent = false;
                dramctrl_rep->birthtime = system_time;
                dramctrl_rep->per_mem_instr_stats = per_mem_instr_stats;
            }

            if (!dramctrl_rep->sent) {
                m_dramctrl_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
            } else {
                m_dramctrl_work_table.erase(it_addr++);
                continue;
            }
        } else {
            ++it_addr;
            continue;;
        }

    }

}

void sharedSharedEMRA::accept_incoming_messages() {

    if (m_core_receive_queues[MSG_RA_REP]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_RA_REP]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        maddr_t msg_start_maddr = get_start_maddr_in_line(data_msg->maddr);
        mh_assert(m_work_table.count(msg_start_maddr) && 
                  (m_work_table[msg_start_maddr]->status == _WAIT_RA_REP));
        m_work_table[msg_start_maddr]->ra_rep = data_msg;
        m_core_receive_queues[MSG_RA_REP]->pop();
    }
    
    if (m_core_receive_queues[MSG_RA_REQ]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_RA_REQ]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_work_table_entry_schedule_q.push_back(make_tuple(true, data_msg));
    }

    if (m_core_receive_queues[MSG_DRAMCTRL_REQ]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REQ]->front();
            std::shared_ptr<dramctrlMsg> dram_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        m_dramctrl_req_schedule_q.push_back(make_tuple(true, dram_msg));
    }

    if (m_core_receive_queues[MSG_DRAMCTRL_REP]->size()) {
        /* note: no replies for DRAM writes */
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REP]->front();
            std::shared_ptr<dramctrlMsg> dramctrl_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        maddr_t start_maddr = dramctrl_msg->dram_req->maddr(); /* always access by a cache line */
        mh_assert(m_work_table.count(start_maddr) > 0);
        m_work_table[start_maddr]->dramctrl_rep = dramctrl_msg;
        m_core_receive_queues[MSG_DRAMCTRL_REP]->pop();
    }

}

void sharedSharedEMRA::schedule_requests() {

    static boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    /*****************************/
    /* scheduling for core ports */
    /*****************************/

    /* if a request from a core cannot get a core-to-memory port, it never compete for a work table entry. */
    /* therefore, core requests need 2-step arbitration */
    random_shuffle(m_core_port_schedule_q.begin(), m_core_port_schedule_q.end(), rr_fn);
    uint32_t requested = 0;
    while (m_core_port_schedule_q.size()) {
            std::shared_ptr<memoryRequest> req = m_core_port_schedule_q.front();
        if (requested < m_available_core_ports) {
            m_new_work_table_entry_schedule_q.push_back(make_tuple(false, req));
            ++requested;
        } else {
            set_req_status(req, REQ_RETRY);
        }
        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
    }

    /*******************************************/
    /* scheduling for empty work table entries */
    /*******************************************/

    random_shuffle(m_new_work_table_entry_schedule_q.begin(), m_new_work_table_entry_schedule_q.end(), rr_fn);
    while (m_new_work_table_entry_schedule_q.size()) {
        bool is_remote = get<0>(m_new_work_table_entry_schedule_q.front());
        if (is_remote) {
                std::shared_ptr<coherenceMsg> ra_req = static_pointer_cast<coherenceMsg>(get<1>(m_new_work_table_entry_schedule_q.front()));
            maddr_t start_maddr = get_start_maddr_in_line(ra_req->maddr);
            std::shared_ptr<sharedSharedEMRAStatsPerMemInstr> per_mem_instr_stats = ra_req->per_mem_instr_stats;

            if (m_work_table.count(start_maddr) || m_work_table_vacancy == 0) {
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());
                continue;
            } 

            bool is_read = (ra_req->type == RA_READ_REQ);

            std::shared_ptr<cacheRequest> l1_req (new cacheRequest(ra_req->maddr,
                                                              (is_read)? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                                              ra_req->word_count,
                                                              (is_read)? shared_array<uint32_t>() : ra_req->data));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(false);
            l1_req->set_claim(false);
            l1_req->set_evict(false);

            std::shared_ptr<tableEntry> new_entry(new tableEntry);
            new_entry->core_req = std::shared_ptr<memoryRequest>();
            new_entry->cat_req = std::shared_ptr<catRequest>();
            new_entry->l1_req = l1_req; /* valid */
            new_entry->l2_req = std::shared_ptr<cacheRequest>();
            new_entry->ra_req = ra_req; /* valid */
            new_entry->ra_rep = std::shared_ptr<coherenceMsg>();
            new_entry->dramctrl_req = std::shared_ptr<dramctrlMsg>();
            new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
            new_entry->per_mem_instr_stats = per_mem_instr_stats; /* valid */

            new_entry->status = _L1_FOR_REMOTE;

            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l1_write_req_schedule_q.push_back(new_entry);
            }

            m_work_table[start_maddr] = new_entry;
            --m_work_table_vacancy;

            /* pop from the network receive queue */
            if (stats_enabled() && per_mem_instr_stats) {
                new_entry->per_mem_instr_stats->add_ra_req_nas(system_time - ra_req->birthtime);
            }
            m_core_receive_queues[MSG_RA_REQ]->pop();

            m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());

            mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                      << " ] A remote request from " << ra_req->sender << " on " << ra_req->maddr << " got into the table " << endl;

        } else {
                std::shared_ptr<memoryRequest> core_req = static_pointer_cast<memoryRequest>(get<1>(m_new_work_table_entry_schedule_q.front()));
            maddr_t start_maddr = get_start_maddr_in_line(core_req->maddr());
            std::shared_ptr<sharedSharedEMRAStatsPerMemInstr> per_mem_instr_stats = 
                (core_req->per_mem_instr_runtime_info()) ? 
                    static_pointer_cast<sharedSharedEMRAStatsPerMemInstr>(*(core_req->per_mem_instr_runtime_info()))
                :
                    std::shared_ptr<sharedSharedEMRAStatsPerMemInstr>();

            if (m_work_table.count(start_maddr) || m_work_table_vacancy == 0) {
                set_req_status(core_req, REQ_RETRY);
                m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());
                continue;
            }

            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_mem_srz(system_time - per_mem_instr_stats->serialization_begin_time_at_current_core());
            }

            std::shared_ptr<catRequest> cat_req(new catRequest(core_req->maddr(), m_id));
            cat_req->set_serialization_begin_time(system_time);

            std::shared_ptr<cacheRequest> l1_req(new cacheRequest(core_req->maddr(),
                                                             core_req->is_read()? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                                             core_req->word_count(),
                                                             core_req->is_read()? shared_array<uint32_t>() : core_req->data()));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(false);
            l1_req->set_claim(false);
            l1_req->set_evict(false);

            std::shared_ptr<tableEntry> new_entry(new tableEntry);
            new_entry->core_req = core_req; /* valid */
            new_entry->cat_req = cat_req; /* valid */
            new_entry->l1_req = l1_req; /* valid */
            new_entry->l2_req = std::shared_ptr<cacheRequest>();
            new_entry->ra_req = std::shared_ptr<coherenceMsg>();
            new_entry->ra_rep = std::shared_ptr<coherenceMsg>();
            new_entry->dramctrl_req = std::shared_ptr<dramctrlMsg>();
            new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
            new_entry->per_mem_instr_stats = per_mem_instr_stats; /* valid */

            new_entry->status = _CAT_AND_L1_FOR_LOCAL;

            m_cat_req_schedule_q.push_back(new_entry);
            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l1_write_req_schedule_q.push_back(new_entry);
            }

            m_work_table[start_maddr] = new_entry;
            --m_work_table_vacancy;
            --m_available_core_ports;

            m_new_work_table_entry_schedule_q.erase(m_new_work_table_entry_schedule_q.begin());

            mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                      << " ] A core request on " << core_req->maddr() << " got into the table " << endl;
        }
    }
    m_new_work_table_entry_schedule_q.clear();

    /************************************/
    /* scheduling for dramctrl requests */
    /************************************/
    
    random_shuffle(m_dramctrl_req_schedule_q.begin(), m_dramctrl_req_schedule_q.end(), rr_fn);
    while (m_dramctrl_req_schedule_q.size()) {
        bool is_remote = get<0>(m_dramctrl_req_schedule_q.front());
        std::shared_ptr<tableEntry> entry = std::shared_ptr<tableEntry>();
        std::shared_ptr<dramctrlMsg> dramctrl_msg = std::shared_ptr<dramctrlMsg>();
        if (is_remote) {
            dramctrl_msg = static_pointer_cast<dramctrlMsg>(get<1>(m_dramctrl_req_schedule_q.front()));
        } else {
            entry = static_pointer_cast<tableEntry>(get<1>(m_dramctrl_req_schedule_q.front()));
            dramctrl_msg = entry->dramctrl_req;
            if (m_dramctrl_writeback_status.count(entry->dramctrl_req->maddr) && 
                m_dramctrl_writeback_status[entry->dramctrl_req->maddr] != entry) 
            {
                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                          << " ] A local DRAM read request on " << dramctrl_msg->maddr << " blocked by a writeback " << endl;
                
                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());
                continue;
            }
        }

        if (m_dramctrl_location == m_id) {
            if (m_dramctrl->available()) {
                if (m_dramctrl_work_table.count(dramctrl_msg->maddr)) {
                    m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());
                    continue;
                }
                if (dramctrl_msg->dram_req->is_read()) {
                        std::shared_ptr<dramctrlTableEntry> new_entry(new dramctrlTableEntry);
                    new_entry->dramctrl_req = dramctrl_msg;
                    new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
                    new_entry->per_mem_instr_stats = dramctrl_msg->per_mem_instr_stats;
                    new_entry->operation_begin_time= system_time;

                    m_dramctrl_work_table[dramctrl_msg->maddr] = new_entry;

                    if (is_remote && stats_enabled() && new_entry->per_mem_instr_stats) {
                        new_entry->per_mem_instr_stats->add_dramctrl_req_nas(system_time - dramctrl_msg->birthtime);
                    }
                    mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                              << " ] A DRAM read request on " << dramctrl_msg->maddr << " got into the table " << endl;
                } else {
                    /* if write, nothing else to do */
                    mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                              << " ] A DRAM write request on " << dramctrl_msg->maddr << " left for DRAM " << endl;
                }

                m_dramctrl->request(dramctrl_msg->dram_req);

                if (stats_enabled()) {
                    stats()->add_dram_action();
                }

                dramctrl_msg->sent = true;

                if (is_remote) {
                    m_core_receive_queues[MSG_DRAMCTRL_REQ]->pop();
                }

                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());

            } else {
                break;
            }
        } else {
            if (m_core_send_queues[MSG_DRAMCTRL_REQ]->available()) {

                    std::shared_ptr<message_t> msg(new message_t);
                msg->src = m_id;
                msg->dst = m_dramctrl_location;
                msg->type = MSG_DRAMCTRL_REQ;
                msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                msg->content = dramctrl_msg;

                m_core_send_queues[MSG_DRAMCTRL_REQ]->push_back(msg);

                dramctrl_msg->sent = true;

                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] DRAMCTRL req sent " 
                          << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;
                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());

            } else {
                break;
            }
        }
    }
    m_dramctrl_req_schedule_q.clear();
    m_dramctrl_writeback_status.clear();

    /**********************/
    /* scheduling for CAT */
    /**********************/

    random_shuffle(m_cat_req_schedule_q.begin(), m_cat_req_schedule_q.end(), rr_fn);
    while (m_cat->available() && m_cat_req_schedule_q.size()) {
            std::shared_ptr<tableEntry> entry = m_cat_req_schedule_q.front();
            std::shared_ptr<catRequest> cat_req = entry->cat_req;

        if (cat_req->serialization_begin_time() == UINT64_MAX) {
            cat_req->set_operation_begin_time(UINT64_MAX);
        } else {
            cat_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_CAT)->add_cat_srz(system_time - cat_req->serialization_begin_time());
            }
        }

        m_cat->request(cat_req);

        if (stats_enabled()) {
            stats()->add_cat_action();
        }
        m_cat_req_schedule_q.erase(m_cat_req_schedule_q.begin());
    }
    m_cat_req_schedule_q.clear();

    /*********************/
    /* scheduling for L1 */
    /*********************/

    random_shuffle(m_l1_read_req_schedule_q.begin(), m_l1_read_req_schedule_q.end(), rr_fn);
    while (m_l1->read_port_available() && m_l1_read_req_schedule_q.size()) {
            std::shared_ptr<tableEntry>& entry = m_l1_read_req_schedule_q.front();
            std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        
        m_l1->request(l1_req);

        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        m_l1_read_req_schedule_q.erase(m_l1_read_req_schedule_q.begin());
    }
    m_l1_read_req_schedule_q.clear();

    random_shuffle(m_l1_write_req_schedule_q.begin(), m_l1_write_req_schedule_q.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q.size()) {
            std::shared_ptr<tableEntry>& entry = m_l1_write_req_schedule_q.front();
            std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        
        m_l1->request(l1_req);

        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        m_l1_write_req_schedule_q.erase(m_l1_write_req_schedule_q.begin());
    }
    m_l1_write_req_schedule_q.clear();
   
    /*********************/
    /* scheduling for L2 */
    /*********************/

    random_shuffle(m_l2_read_req_schedule_q.begin(), m_l2_read_req_schedule_q.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q.size()) {
            std::shared_ptr<tableEntry>& entry = m_l2_read_req_schedule_q.front();
            std::shared_ptr<cacheRequest>& l2_req = entry->l2_req;

        if (m_l2_writeback_status.count(get_start_maddr_in_line(l2_req->maddr())) > 0) {
            m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
            continue;
        }

        if (l2_req->serialization_begin_time() == UINT64_MAX) {
            l2_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l2_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        
        m_l2->request(l2_req);

        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
    }
    m_l2_read_req_schedule_q.clear();

    random_shuffle(m_l2_write_req_schedule_q.begin(), m_l2_write_req_schedule_q.end(), rr_fn);
    while (m_l2->write_port_available() && m_l2_write_req_schedule_q.size()) {
            std::shared_ptr<tableEntry>& entry = m_l2_write_req_schedule_q.front();
            std::shared_ptr<cacheRequest>& l2_req = entry->l2_req;

        if (m_l2_writeback_status.count(get_start_maddr_in_line(l2_req->maddr())) > 0 &&
            m_l2_writeback_status[get_start_maddr_in_line(l2_req->maddr())] != entry)
        {
            m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
            continue;
        }

        if (l2_req->serialization_begin_time() == UINT64_MAX) {
            l2_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l2_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        
        m_l2->request(l2_req);

        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
    }
    m_l2_write_req_schedule_q.clear();

    /* a writeback is a must-hit. once requested don't worry about other reads/writes are requested after it */
    m_l2_writeback_status.clear();

    /**************************************/
    /* scheduling for sending ra requests */
    /**************************************/

    random_shuffle(m_ra_req_schedule_q.begin(), m_ra_req_schedule_q.end(), rr_fn);
    while (m_ra_req_schedule_q.size()) {
            std::shared_ptr<tableEntry>& entry = m_ra_req_schedule_q.front();
            std::shared_ptr<coherenceMsg>& ra_req = entry->ra_req;

        if (m_core_send_queues[MSG_RA_REQ]->available()) {

                std::shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = ra_req->receiver;
            msg->type = MSG_RA_REQ;
            if (ra_req->type == RA_READ_REQ) {
                msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
            } else {
                msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + ra_req->word_count * 4);
            }
            msg->content = ra_req;

            m_core_send_queues[MSG_RA_REQ]->push_back(msg);

            ra_req->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] ra req sent " 
                << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;

            m_ra_req_schedule_q.erase(m_ra_req_schedule_q.begin());

        } else {
            break;
        }
    }
    m_ra_req_schedule_q.clear();

    /*************************************/
    /* scheduling for sending ra replies */
    /*************************************/
    random_shuffle(m_ra_rep_schedule_q.begin(), m_ra_rep_schedule_q.end(), rr_fn);
    while (m_ra_rep_schedule_q.size()) {
            std::shared_ptr<tableEntry>& entry = m_ra_rep_schedule_q.front();
            std::shared_ptr<coherenceMsg>& ra_rep = entry->ra_rep;

        if (m_core_send_queues[MSG_RA_REP]->available()) {

                std::shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = ra_rep->receiver;
            msg->type = MSG_RA_REP;
            msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + ra_rep->word_count * 4);
            msg->content = ra_rep;

            m_core_send_queues[MSG_RA_REP]->push_back(msg);

            ra_rep->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] ra rep sent " 
                      << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;
            m_ra_rep_schedule_q.erase(m_ra_rep_schedule_q.begin());

        } else {
            break;
        }
    }
    m_ra_rep_schedule_q.clear();

    /*******************************************/
    /* scheduling for sending dramctrl replies */
    /*******************************************/

    random_shuffle(m_dramctrl_rep_schedule_q.begin(), m_dramctrl_rep_schedule_q.end(), rr_fn);
    while (m_dramctrl_rep_schedule_q.size()) {
            std::shared_ptr<dramctrlTableEntry>& entry = m_dramctrl_rep_schedule_q.front();
            std::shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
            std::shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;

        if (dramctrl_req->sender == m_id) {
            mh_assert(m_work_table.count(dramctrl_req->maddr));
            m_work_table[dramctrl_req->maddr]->dramctrl_rep = entry->dramctrl_rep;
            mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] has sent a DRAMCTRL rep for address " 
                      << dramctrl_rep->maddr << " to core " << m_id << endl;
            dramctrl_rep->sent = true;

        } else if (m_core_send_queues[MSG_DRAMCTRL_REP]->available()) {

                std::shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = dramctrl_req->sender;
            msg->type = MSG_DRAMCTRL_REP;
            msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
            msg->content = dramctrl_rep;

            m_core_send_queues[MSG_DRAMCTRL_REP]->push_back(msg);

            dramctrl_rep->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] dramctrl rep sent " 
                      << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;

        }

        m_dramctrl_rep_schedule_q.erase(m_dramctrl_rep_schedule_q.begin());

    }

}

