// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSI.hpp"
#include "messages.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>

#define PRINT_PROGRESS
//#undef PRINT_PROGRESS

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

/* Used for tentative stats per memory instruction (outstanding costs survive) */
#define T_IDX_CAT 0
#define T_IDX_L1 1
#define T_IDX_L2 2

/***********************************************************/
/* Cache helper functions to customize the cache behaviors */
/***********************************************************/

/* coherence info copier */

static shared_ptr<void> cache_copy_coherence_info(shared_ptr<void> source) {
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> ret
        (new privateSharedMSI::cacheCoherenceInfo(*static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(source)));
    return ret;
}

static shared_ptr<void> dir_copy_coherence_info(shared_ptr<void> source) {
    shared_ptr<privateSharedMSI::dirCoherenceInfo> ret
        (new privateSharedMSI::dirCoherenceInfo(*static_pointer_cast<privateSharedMSI::dirCoherenceInfo>(source)));
    return ret;
}

/* hit checkers */

static bool cache_hit_checker(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) {

    shared_ptr<privateSharedMSI::cacheCoherenceInfo> cache_coherence_info = 
        static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(line.coherence_info);
    shared_ptr<privateSharedMSI::cacheAuxInfoForCoherence> request_info = 
        static_pointer_cast<privateSharedMSI::cacheAuxInfoForCoherence>(req->aux_info_for_coherence());

    if (*request_info == privateSharedMSI::LOCAL_WRITE) {
        if (cache_coherence_info->status == privateSharedMSI::SHARED) {
            return false;
        }
    } else if (*request_info == privateSharedMSI::UPDATE_BY_SHREP) {
        cache_coherence_info->status = privateSharedMSI::SHARED;
    } else if (*request_info == privateSharedMSI::UPDATE_BY_EXREP) {
        cache_coherence_info->status = privateSharedMSI::EXCLUSIVE;
    }

    return true;

}

static bool dir_hit_checker(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) {

    /* do both test checks and directory updates */

    shared_ptr<privateSharedMSI::dirCoherenceInfo> dir_coherence_info = 
        static_pointer_cast<privateSharedMSI::dirCoherenceInfo>(line.coherence_info);
    shared_ptr<privateSharedMSI::dirAuxInfoForCoherence> request_info = 
        static_pointer_cast<privateSharedMSI::dirAuxInfoForCoherence>(req->aux_info_for_coherence());

    request_info->initial_dir_info = 
        static_pointer_cast<privateSharedMSI::dirCoherenceInfo>(dir_copy_coherence_info(line.coherence_info));

    if (request_info->req_type == privateSharedMSI::READ_FOR_SHREQ) {

        bool is_hit = !(dir_coherence_info->status == privateSharedMSI::EXCLUSIVE && 
                        dir_coherence_info->dir.count(request_info->core_id) == 0);

        if (dir_coherence_info->status == privateSharedMSI::EXCLUSIVE) {
            dir_coherence_info->status = privateSharedMSI::SHARED;
        }
        dir_coherence_info->dir.insert(request_info->core_id);

        return is_hit;

    } else if (request_info->req_type == privateSharedMSI::READ_FOR_EXREQ) {

        bool is_hit = dir_coherence_info->dir.size() == 0 ||
                      (dir_coherence_info->dir.size() == 1 && dir_coherence_info->dir.count(request_info->core_id));

        dir_coherence_info->status = privateSharedMSI::EXCLUSIVE;
        dir_coherence_info->dir.clear();
        dir_coherence_info->dir.insert(request_info->core_id);

        return is_hit;

    } else if (request_info->req_type == privateSharedMSI::UPDATE_FOR_INVREP_OR_FLUSHREP) {

        dir_coherence_info->dir.erase(request_info->core_id);
        if (dir_coherence_info->dir.size() == 0) {
            dir_coherence_info->status = privateSharedMSI::SHARED;
        }

    } else if (request_info->req_type == privateSharedMSI::UPDATE_FOR_WBREP) {

        dir_coherence_info->status = privateSharedMSI::SHARED;

    } else if (request_info->req_type == privateSharedMSI::UPDATE_FOR_EMPTYREQ) {

        mh_assert(dir_coherence_info->locked);

        if (line.data_dirty) {
            request_info->is_replaced_line_dirty = true;
            request_info->replaced_line = shared_array<uint32_t>(new uint32_t[req->word_count()]);
            for (uint32_t i = 0; i < req->word_count(); ++i) {
                request_info->replaced_line[i] = line.data[i];
            }
        }
        /* immediately switch to the evicting line */
        line.start_maddr = request_info->replacing_maddr;
    }

    return true;

}

/* evictable checker */

static bool dir_can_evict_line(cacheLine &line, const uint64_t& system_time) {
    shared_ptr<privateSharedMSI::dirCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::dirCoherenceInfo>(line.coherence_info);
    if (info->locked) 
        return false;
    return true;
}

static bool dir_evict_need_action(cacheLine &line, const uint64_t& system_time) {

    shared_ptr<privateSharedMSI::dirCoherenceInfo> dir_coherence_info = 
        static_pointer_cast<privateSharedMSI::dirCoherenceInfo>(line.coherence_info);
    if (dir_coherence_info->dir.size()) {
        dir_coherence_info->locked = true;
        return true;
    } else {
        return false;
    }

}

privateSharedMSI::privateSharedMSI(uint32_t id,
                                   const uint64_t &t,
                                   shared_ptr<tile_statistics> st,
                                   logger &l,
                                   shared_ptr<random_gen> r,
                                   shared_ptr<cat> a_cat,
                                   privateSharedMSICfg_t cfg) :
    memory(id, t, st, l, r),
    m_cfg(cfg),
    m_l1(NULL),
    m_l2(NULL),
    m_cat(a_cat),
    m_stats(shared_ptr<privateSharedMSIStatsPerTile>()),
    m_cache_table_vacancy(cfg.cache_table_size),
    m_dir_table_vacancy_shared(cfg.dir_table_size_shared),
    m_dir_table_vacancy_cache_rep_exclusive(cfg.dir_table_size_cache_rep_exclusive),
    m_dir_table_vacancy_empty_req_exclusive(cfg.dir_table_size_empty_req_exclusive),
    m_available_core_ports(cfg.num_local_core_ports)
{
    /* sanity checks */
    if (m_cfg.bytes_per_flit == 0) throw err_bad_shmem_cfg("flit size must be non-zero.");
    if (m_cfg.words_per_cache_line == 0) throw err_bad_shmem_cfg("cache line size must be non-zero.");
    if (m_cfg.lines_in_l1 == 0) throw err_bad_shmem_cfg("privateSharedMSI : L1 size must be non-zero.");
    if (m_cfg.lines_in_l2 == 0) throw err_bad_shmem_cfg("privateSharedMSI : L2 size must be non-zero.");
    if (m_cfg.cache_table_size == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI : cache table size must be non-zero.");
    if (m_cfg.dir_table_size_shared == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI : shared directory table size must be non-zero.");
    if (m_cfg.dir_table_size_cache_rep_exclusive == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI : cache reply exclusive work table size must be non-zero.");
    if (m_cfg.dir_table_size_empty_req_exclusive == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI : empty request exclusive work table size must be non-zero.");
    
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

    m_l1->set_helper_copy_coherence_info(&cache_copy_coherence_info);
    m_l1->set_helper_is_coherence_hit(&cache_hit_checker);

    m_l2->set_helper_copy_coherence_info(&dir_copy_coherence_info);
    m_l2->set_helper_is_coherence_hit(&dir_hit_checker);
    m_l2->set_helper_can_evict_line(&dir_can_evict_line);
    m_l2->set_helper_evict_need_action(&dir_evict_need_action);
    
    m_cat_req_schedule_q.reserve(m_cfg.cache_table_size);
    m_l1_read_req_schedule_q.reserve(m_cfg.cache_table_size);
    m_l1_write_req_schedule_q.reserve(m_cfg.cache_table_size);
    m_cache_req_schedule_q.reserve(m_cfg.cache_table_size);
    m_cache_rep_schedule_q.reserve(m_cfg.cache_table_size);

    uint32_t total_dir_table_size = m_cfg.dir_table_size_shared + m_cfg.dir_table_size_cache_rep_exclusive
                                    + m_cfg.dir_table_size_empty_req_exclusive;
    m_l2_read_req_schedule_q.reserve(total_dir_table_size);
    m_l2_write_req_schedule_q.reserve(total_dir_table_size);
    m_dir_req_schedule_q.reserve(total_dir_table_size);
    m_dir_rep_schedule_q.reserve(total_dir_table_size);
    m_dramctrl_req_schedule_q.reserve(total_dir_table_size);
    m_dramctrl_rep_schedule_q.reserve(total_dir_table_size);

}

privateSharedMSI::~privateSharedMSI() {
    delete m_l1;
    delete m_l2;
}

uint32_t privateSharedMSI::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

void privateSharedMSI::request(shared_ptr<memoryRequest> req) {

    /* assumes a request is not across multiple cache lines */
    uint32_t __attribute__((unused)) byte_offset = req->maddr().address%(m_cfg.words_per_cache_line*4);
    mh_assert( (byte_offset + req->word_count()*4) <= m_cfg.words_per_cache_line * 4);

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    /* per memory instruction info */
    shared_ptr<shared_ptr<void> > p_runtime_info = req->per_mem_instr_runtime_info();
    shared_ptr<void>& runtime_info = *p_runtime_info;
    shared_ptr<privateSharedMSIStatsPerMemInstr> per_mem_instr_stats;
    if (!runtime_info) {
        /* no per-instr stats: this is the first time this memory instruction is issued */
        per_mem_instr_stats = shared_ptr<privateSharedMSIStatsPerMemInstr>(new privateSharedMSIStatsPerMemInstr(req->is_read()));
        per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
        runtime_info = per_mem_instr_stats;
    } else {
        per_mem_instr_stats = 
            static_pointer_cast<privateSharedMSIStatsPerMemInstr>(*req->per_mem_instr_runtime_info());
        if (per_mem_instr_stats->is_in_migration()) {
            per_mem_instr_stats = static_pointer_cast<privateSharedMSIStatsPerMemInstr>(runtime_info);
            per_mem_instr_stats->migration_finished(system_time, stats_enabled());
            per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
        }
    }

    /* will schedule for a core port and a work table entry in schedule function */
    m_core_port_schedule_q.push_back(req);

}

void privateSharedMSI::tick_positive_edge() {
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
        cerr << " in cache table : " << m_cache_table.size() 
             << " in directory table : " << m_dir_table.size() << endl;
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

void privateSharedMSI::tick_negative_edge() {

    m_l1->tick_negative_edge();
    m_l2->tick_negative_edge();
    m_cat->tick_negative_edge();
    if(m_dramctrl) {
        m_dramctrl->tick_negative_edge();
    }

    /* accept messages and write into tables */
    accept_incoming_messages();

    update_cache_table();

    update_dir_table();

    update_dramctrl_work_table();

}

void privateSharedMSI::update_cache_table() {

    for (cacheTable::iterator it_addr = m_cache_table.begin(); it_addr != m_cache_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        shared_ptr<cacheTableEntry>& entry = it_addr->second;

        shared_ptr<memoryRequest>& core_req = entry->core_req;
        shared_ptr<catRequest>& cat_req = entry->cat_req;

        shared_ptr<cacheRequest>& l1_req = entry->l1_req;
        shared_ptr<coherenceMsg>& dir_req = entry->dir_req;
        shared_ptr<coherenceMsg>& dir_rep = entry->dir_rep;
        shared_ptr<coherenceMsg>& cache_req = entry->cache_req;
        shared_ptr<coherenceMsg>& cache_rep = entry->cache_rep;

        shared_ptr<cacheLine> l1_line = (l1_req)? l1_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> l1_victim = (l1_req)? l1_req->line_to_evict_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheCoherenceInfo> l1_line_info = 
            (l1_line)? static_pointer_cast<cacheCoherenceInfo>(l1_line->coherence_info) : shared_ptr<cacheCoherenceInfo>();
        shared_ptr<cacheCoherenceInfo> l1_victim_info = 
            (l1_victim)? static_pointer_cast<cacheCoherenceInfo>(l1_victim->coherence_info) : shared_ptr<cacheCoherenceInfo>();

        shared_ptr<privateSharedMSIStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        if (entry->status == _CACHE_CAT_AND_L1_FOR_LOCAL) {

            uint32_t home;

            if (l1_req->status() == CACHE_REQ_HIT) {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 HIT and finish serving address "
                          << core_req->maddr() << endl;

                home = l1_line_info->home;
                cat_req = shared_ptr<catRequest>();

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l1_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);
                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;

                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                        per_mem_instr_stats->add_l1_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                    if (core_req->is_read()) {
                        stats()->hit_for_read_instr_at_l1();
                        stats()->did_finish_read();
                    } else {
                        stats()->hit_for_write_instr_at_l1();
                        stats()->did_finish_write();
                    }
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }

                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISHED */

            }  
            
            if (l1_req->status() == CACHE_REQ_MISS && l1_line) {

                mh_assert(!core_req->is_read());
                mh_assert(l1_line_info->status == SHARED);

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a write-on-shared miss on address "
                          << core_req->maddr() << endl;

                home = l1_line_info->home;
                cat_req = shared_ptr<catRequest>();

                /* could migrate out here */

                cache_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_req->sender = m_id;
                cache_req->receiver = home;
                cache_req->type = core_req->is_read()? SH_REQ : EX_REQ;
                cache_req->maddr = start_maddr;
                cache_req->sent = false;
                cache_req->per_mem_instr_stats = per_mem_instr_stats;
                cache_req->word_count = m_cfg.words_per_cache_line;
                cache_req->data = shared_array<uint32_t>();
                cache_req->birthtime = system_time;

                cache_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_rep->sender = m_id;
                cache_rep->receiver = home;
                cache_rep->type = INV_REP;
                cache_rep->maddr = start_maddr;
                cache_rep->sent = false;
                cache_rep->per_mem_instr_stats = per_mem_instr_stats;
                cache_rep->birthtime = system_time;
                cache_rep->word_count = 0;
                cache_rep->data = shared_array<uint32_t>();

                if (stats_enabled()) {
                    stats()->evict_at_l1();
                }

                m_cache_rep_schedule_q.push_back(entry);

                entry->status = _CACHE_SEND_CACHE_REP;
                entry->substatus = _CACHE_SEND_CACHE_REP__SWITCH;

                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                        per_mem_instr_stats->add_l1_cost_for_write_on_shared_miss
                            (per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                    stats()->write_on_shared_miss_for_write_instr_at_l1();
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }

                ++it_addr;
                continue;
                /* TRANSITION */

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
                    /* cannot continue without CAT info */
                    home = cat_req->home();

                    if (l1_req->status() == CACHE_REQ_MISS) {

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a true miss on address "
                                  << core_req->maddr() << endl;

                        /* could migrate out here */
                        
                        cache_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                        cache_req->sender = m_id;
                        cache_req->receiver = home;
                        cache_req->type = core_req->is_read()? SH_REQ : EX_REQ;
                        cache_req->maddr = start_maddr;
                        cache_req->sent = false;
                        cache_req->per_mem_instr_stats = per_mem_instr_stats;
                        cache_req->word_count = m_cfg.words_per_cache_line;
                        cache_req->data = shared_array<uint32_t>();
                        cache_req->birthtime = system_time;

                        m_cache_req_schedule_q.push_back(entry);

                        entry->status = _CACHE_SEND_CACHE_REQ;

                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                if (per_mem_instr_stats->get_max_tentative_data_index() == T_IDX_L1) {
                                    per_mem_instr_stats->add_l1_cost_for_true_miss
                                        (per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                                }
                                per_mem_instr_stats->commit_max_tentative_data();
                            }
                            if (core_req->is_read()) {
                                stats()->true_miss_for_read_instr_at_l1();
                            } else {
                                stats()->true_miss_for_write_instr_at_l1();
                            }
                        } else if (per_mem_instr_stats) {
                            per_mem_instr_stats->clear_tentative_data();
                        }
                            
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    }
                }

                if (l1_req->status() == CACHE_REQ_NEW) {
                    /* the L1 request has lost an arbitration - retry */
                    if (l1_req->use_read_ports()) {
                        m_l1_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l1_write_req_schedule_q.push_back(entry);
                    }
                }

                if (cat_req->status() == CAT_REQ_NEW) {
                    /* the cat request has lost an arbitration - retry */
                    m_cat_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            }

            /* _CACHE_CAT_AND_L1_FOR_LOCAL */

        } else if (entry->status == _CACHE_L1_FOR_DIR_REQ) {

            if (l1_req->status() == CACHE_REQ_NEW) {
                /* the L1 request has lost an arbitration - retry */
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
                per_mem_instr_stats->clear_tentative_data();
            }

            if (l1_req->status() == CACHE_REQ_HIT) {

                uint32_t home = l1_line_info->home;
                cache_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_rep->sender = m_id;
                cache_rep->receiver = home;
                if (dir_req->type == INV_REQ) {
                    cache_rep->type = INV_REP;
                    if (stats_enabled()) {
                        stats()->evict_at_l1();
                    }
                } else if (dir_req->type == FLUSH_REQ) {
                    cache_rep->type = FLUSH_REP;
                    if (stats_enabled()) {
                        stats()->evict_at_l1();
                    }
                } else {
                    cache_rep->type = WB_REP;
                }

                cache_rep->maddr = start_maddr;
                cache_rep->sent = false;
                cache_rep->per_mem_instr_stats = per_mem_instr_stats;
                cache_rep->birthtime = system_time;

                if (m_cfg.use_mesi) {
                    if (!l1_line->data_dirty) {
                        cache_rep->word_count = 0;
                    } else {
                        if (stats_enabled()) {
                            stats()->writeback_at_l1();
                        }
                        cache_rep->word_count = m_cfg.words_per_cache_line;
                    }
                } else {
                    if (l1_line_info->status == SHARED) {
                        cache_rep->word_count = 0;
                    } else {
                        if (stats_enabled()) {
                            stats()->writeback_at_l1();
                        }
                        cache_rep->word_count = m_cfg.words_per_cache_line;
                    }
                }

                cache_rep->data = l1_line->data;

                m_cache_rep_schedule_q.push_back(entry);

                entry->status = _CACHE_SEND_CACHE_REP;
                entry->substatus = _CACHE_SEND_CACHE_REP__DIR_REQ;
                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                /* line was evicted and a cache reply is already sent */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] discarded a directory request for "
                          << start_maddr << " as it was already invalidated." << endl;
                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
            }

        } else if (entry->status == _CACHE_SEND_CACHE_REQ) {

            if (cache_req->sent) {
                entry->status = _CACHE_WAIT_DIR_REP;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent a cache request (" << cache_req->type
                          << ") to " << cache_req->receiver << " for " << cache_req->maddr << endl;
                ++it_addr;
                continue;
                /* TRANSITION */
            } else {
                m_cache_req_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }
            /* _CACHE_SEND_CACHE_REQ */

        } else if (entry->status == _CACHE_WAIT_DIR_REP) {

            if (!dir_rep) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dir_rep_nas(system_time - dir_rep->birthtime);
            }

            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a directory reply ("
                      << dir_rep->type << ") for " << start_maddr << endl;

            shared_array<uint32_t> data = dir_rep->data;
            if (core_req->is_read()) {
                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = data[i + word_offset];
                }
                set_req_data(core_req, ret);
            } else {
                mh_assert(dir_rep->type == EX_REP);
                uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    data[i + word_offset] = core_req->data()[i];
                }
            }

            shared_ptr<cacheCoherenceInfo> new_info(new cacheCoherenceInfo);
            new_info->home = dir_rep->sender;
            new_info->status = dir_rep->type == SH_REP? SHARED : EXCLUSIVE;

            l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line, 
                                                               dir_rep->data, new_info));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(core_req->is_read());
            l1_req->set_claim(true);
            l1_req->set_evict(true);
            l1_req->set_aux_info_for_coherence
                (shared_ptr<cacheAuxInfoForCoherence>
                    (new cacheAuxInfoForCoherence((dir_rep->type == SH_REP)? UPDATE_BY_SHREP : UPDATE_BY_EXREP)));

            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(entry);
            } else {
                m_l1_write_req_schedule_q.push_back(entry);
            }

            entry->status = _CACHE_UPDATE_L1;
            ++it_addr;
            continue;
            /* TRANSITION */

            /* _CACHE_WAIT_DIR_REP */

        } else if (entry->status == _CACHE_UPDATE_L1) {

            if (l1_req->status() == CACHE_REQ_NEW) {
                /* the L1 request has lost an arbitration - retry */
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
            } else {
                mh_assert(l1_req->status() == CACHE_REQ_HIT);

                if (per_mem_instr_stats) {
                    if (stats_enabled()) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                        per_mem_instr_stats->add_l1_cost_for_feed(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    } else {
                        per_mem_instr_stats->clear_tentative_data();
                    }
                }

                if (!l1_victim) {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] is updated for " << start_maddr << " finishing serve"
                              << endl;
                    if (stats_enabled()) {
                        if (core_req->is_read()) {
                            stats()->did_finish_read();
                        } else {
                            stats()->did_finish_write();
                        }
                    }
                    set_req_status(core_req, REQ_DONE);
                    ++m_available_core_ports;
                    ++m_cache_table_vacancy;
                    m_cache_table.erase(it_addr++);
                    continue;
                    /* FINISHED */
                }

                /* we have a victim */

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] is trying to evict a cache line " << l1_victim->start_maddr
                          << endl;

                if (stats_enabled()) {
                    stats()->evict_at_l1();
                }

                cache_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_rep->sender = m_id;
                cache_rep->receiver = l1_victim_info->home;
                if (l1_victim_info->status == SHARED) {
                    cache_rep->type = INV_REP;
                } else {
                    cache_rep->type = FLUSH_REP;
                }

                cache_rep->maddr = l1_victim->start_maddr;
                cache_rep->sent = false;
                cache_rep->per_mem_instr_stats = shared_ptr<privateSharedMSIStatsPerMemInstr>();
                cache_rep->birthtime = system_time;

                if (m_cfg.use_mesi) {
                    if (!l1_victim->data_dirty) {
                        cache_rep->word_count = 0;
                    } else {
                        if (stats_enabled()) {
                            stats()->writeback_at_l1();
                        }
                        cache_rep->word_count = m_cfg.words_per_cache_line;
                    }
                } else {
                    if (l1_victim_info->status == SHARED) {
                        cache_rep->word_count = 0;
                    } else {
                        if (stats_enabled()) {
                            stats()->writeback_at_l1();
                        }
                        cache_rep->word_count = m_cfg.words_per_cache_line;
                    }
                }
                cache_rep->data = l1_victim->data;

                m_cache_rep_schedule_q.push_back(entry);

                entry->status = _CACHE_SEND_CACHE_REP;
                entry->substatus = _CACHE_SEND_CACHE_REP__EVICT;
                ++it_addr;
                continue;
                /* TRANSITION */
            }

            /* _CACHE_UPDATE_L1 */
 
        } else if (entry->status == _CACHE_SEND_CACHE_REP) {

            if (!cache_rep->sent) {
                m_cache_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent a cache reply (" << cache_rep->type 
                      << ") for " << cache_rep->maddr << " to " << cache_rep->receiver << endl;

            if (entry->substatus == _CACHE_SEND_CACHE_REP__SWITCH) {
                if (stats_enabled() && per_mem_instr_stats) {
                    per_mem_instr_stats->add_cache_rep_srz_for_switch(system_time - cache_rep->birthtime);
                }
                cache_req->birthtime = system_time;
                m_cache_req_schedule_q.push_back(entry);

                entry->status = _CACHE_SEND_CACHE_REQ;
                ++it_addr;
                continue;
                /* TRANSITION */
            } else if (entry->substatus == _CACHE_SEND_CACHE_REP__EVICT) {
                if (stats_enabled()) {
                    if (core_req->is_read()) {
                        stats()->did_finish_read();
                    } else {
                        stats()->did_finish_write();
                    }
                    
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->add_cache_rep_srz_for_evict(system_time - cache_rep->birthtime);
                    }
                }
                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;
                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISHED */
            } else if (entry->substatus == _CACHE_SEND_CACHE_REP__DIR_REQ) {
                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISHED */
            }

        }

    }

}

void privateSharedMSI::update_dir_table() {

    for (dirTable::iterator it_addr = m_dir_table.begin(); it_addr != m_dir_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        shared_ptr<dirTableEntry>& entry = it_addr->second;

        shared_ptr<coherenceMsg> __attribute__ ((unused)) & cache_req = entry->cache_req;
        shared_ptr<cacheRequest> __attribute__ ((unused)) & l2_req = entry->l2_req;
        shared_ptr<coherenceMsg> __attribute__ ((unused)) & cache_rep = entry->cache_rep;
        vector<shared_ptr<coherenceMsg> > __attribute__ ((unused)) & dir_reqs = entry->dir_reqs;
        shared_ptr<coherenceMsg> __attribute__ ((unused)) & dir_rep = entry->dir_rep;
        shared_ptr<coherenceMsg> __attribute__ ((unused)) & empty_req = entry->empty_req;
        shared_ptr<dramctrlMsg> __attribute__ ((unused)) & dramctrl_req = entry->dramctrl_req;
        shared_ptr<dramctrlMsg> __attribute__ ((unused)) & dramctrl_rep = entry->dramctrl_rep;

        shared_ptr<cacheLine> l2_line = (l2_req)? l2_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> l2_victim = (l2_req)? l2_req->line_to_evict_copy() : shared_ptr<cacheLine>();
        shared_ptr<dirCoherenceInfo> l2_line_info = 
            (l2_line)? static_pointer_cast<dirCoherenceInfo>(l2_line->coherence_info) : shared_ptr<dirCoherenceInfo>();
        shared_ptr<dirCoherenceInfo> l2_victim_info = 
            (l2_victim)? static_pointer_cast<dirCoherenceInfo>(l2_victim->coherence_info) : shared_ptr<dirCoherenceInfo>();
        shared_ptr<dirAuxInfoForCoherence> l2_aux_info = static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence());

        shared_ptr<privateSharedMSIStatsPerMemInstr> __attribute__((unused)) & per_mem_instr_stats = entry->per_mem_instr_stats;

        if (entry->status == _DIR_L2_FOR_CACHE_REQ) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } 
            if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }
            if (l2_req->status() == CACHE_REQ_HIT) {

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a L2 HIT on "
                          << cache_req->maddr << endl;

                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                        if (cache_req->sender == m_id) {
                            per_mem_instr_stats->add_local_l2_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        } else {
                            per_mem_instr_stats->add_remote_l2_cost_for_hit(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        }
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                    }
                    if (cache_req->type == SH_REQ) {
                        if (cache_req->sender == m_id) {
                            stats()->hit_for_read_instr_at_local_l2();
                        } else {
                            stats()->hit_for_read_instr_at_remote_l2();
                        }
                        stats()->hit_for_read_instr_at_l2();
                    } else {
                        if (cache_req->sender == m_id) {
                            stats()->hit_for_write_instr_at_local_l2();
                        } else {
                            stats()->hit_for_write_instr_at_remote_l2();
                        }
                        stats()->hit_for_write_instr_at_l2();
                    }
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }

                if (l2_aux_info->initial_dir_info->dir.count(cache_req->sender)) {
                    /* cache rep/req out of order */

                    entry->cached_dir = *(l2_aux_info->initial_dir_info);

                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] now waits for delayed cache reply on "
                              << cache_req->maddr << endl;

                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->begin_reorder(system_time);
                    }

                    entry->status = _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP;
                    entry->substatus = _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__REORDER;
                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else {
                    dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                    dir_rep->sender = m_id;
                    dir_rep->receiver = cache_req->sender;
                    dir_rep->type = (cache_req->type == SH_REQ)? SH_REP : EX_REP;
                    dir_rep->word_count = m_cfg.words_per_cache_line;
                    dir_rep->maddr = start_maddr;
                    dir_rep->data = l2_line->data;
                    dir_rep->sent = false;
                    dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                    dir_rep->birthtime = system_time;

                    m_dir_rep_schedule_q.push_back(entry);

                    entry->status = _DIR_SEND_DIR_REP;
                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }

            } else {
                /* miss */
                if (!l2_line) {
                    /* true miss */
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 true miss on "
                              << cache_req->maddr << endl;

                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->
                                get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                            if (cache_req->sender == m_id) {
                                per_mem_instr_stats->
                                    add_local_l2_cost_for_true_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            } else {
                                per_mem_instr_stats->
                                    add_remote_l2_cost_for_true_miss(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            }
                            per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                        }
                        if (cache_req->type == SH_REQ) {
                            if (cache_req->sender == m_id) {
                                stats()->true_miss_for_read_instr_at_local_l2();
                            } else {
                                stats()->true_miss_for_read_instr_at_remote_l2();
                            }
                            stats()->true_miss_for_read_instr_at_l2();
                        } else {
                            if (cache_req->sender == m_id) {
                                stats()->true_miss_for_write_instr_at_local_l2();
                            } else {
                                stats()->true_miss_for_write_instr_at_remote_l2();
                            }
                            stats()->true_miss_for_write_instr_at_l2();
                        }
                    } else if (per_mem_instr_stats) {
                        per_mem_instr_stats->clear_tentative_data();
                    }

                    dramctrl_req = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = start_maddr;
                    dramctrl_req->dram_req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                     DRAM_REQ_READ,
                                                                                     m_cfg.words_per_cache_line));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark it as from local */

                    entry->status = _DIR_SEND_DRAMCTRL_REQ;

                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else {
                    /* coherence miss */
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 coherence miss on "
                              << cache_req->maddr << endl;

                    /* cache the current directory */
                    entry->cached_dir = *(l2_aux_info->initial_dir_info);
                    mh_log(4) << "    current directory : ";
                    if (entry->cached_dir.status == SHARED) {
                        mh_log(4) << "SHARED ";
                    } else {
                        mh_log(4) << "EXCLUSIVE ";
                    }
                    for (set<uint32_t>::iterator it = entry->cached_dir.dir.begin(); it != entry->cached_dir.dir.end(); ++it) {
                        mh_log(4) << *it << " ";
                    }
                    mh_log(4) << endl;

                    /* queue up directory requests to send */
                    for (set<uint32_t>::iterator it = entry->cached_dir.dir.begin(); it != entry->cached_dir.dir.end(); ++it) {
                        if (*it == cache_req->sender) {
                            /* a cache rep is coming, need not send a dir req */
                            continue;
                        }
                        shared_ptr<coherenceMsg> new_dir_req(new coherenceMsg);
                        new_dir_req->sender = m_id;
                        new_dir_req->receiver = *it;
                        if (entry->cached_dir.status == SHARED) {
                            new_dir_req->type = INV_REQ;
                        } else if (cache_req->type == SH_REQ) {
                            new_dir_req->type = WB_REQ;
                        } else {
                            new_dir_req->type = FLUSH_REQ;
                        }
                        new_dir_req->word_count = 0;
                        new_dir_req->maddr = start_maddr;
                        new_dir_req->data = shared_array<uint32_t>();
                        new_dir_req->sent = false;
                        new_dir_req->per_mem_instr_stats = shared_ptr<privateSharedMSIStatsPerMemInstr>();
                        new_dir_req->birthtime = system_time;

                        dir_reqs.push_back(new_dir_req);
                    }
                    if (!dir_reqs.empty()) {
                        m_dir_req_schedule_q.push_back(entry);
                    }

                    /* statistics */
                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->
                                get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                            if (entry->cached_dir.status == EXCLUSIVE) {
                                if (cache_req->type == SH_REQ) {
                                    if (cache_req->sender == m_id) {
                                        per_mem_instr_stats->
                                            add_local_l2_cost_for_read_on_exclusive_miss
                                                (per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                                    } else {
                                        per_mem_instr_stats->
                                            add_remote_l2_cost_for_read_on_exclusive_miss
                                                (per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                                    }
                                } else {
                                    if (cache_req->sender == m_id) {
                                        per_mem_instr_stats->
                                            add_local_l2_cost_for_write_on_exclusive_miss
                                                (per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                                    } else {
                                        per_mem_instr_stats->
                                            add_remote_l2_cost_for_write_on_exclusive_miss
                                                (per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                                    }
                                }
                            } else {
                                mh_assert(cache_req->type == EX_REQ);
                                if (cache_req->sender == m_id) {
                                    per_mem_instr_stats->
                                        add_local_l2_cost_for_write_on_shared_miss
                                            (per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                                } else {
                                    per_mem_instr_stats->
                                        add_remote_l2_cost_for_write_on_shared_miss
                                            (per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                                }
                            }
                            per_mem_instr_stats->commit_tentative_data(T_IDX_L2);

                            per_mem_instr_stats->begin_inv_for_coherence(system_time, dir_reqs.size());
                        }

                        if (entry->cached_dir.status == EXCLUSIVE) {
                            if (cache_req->type == SH_REQ) {
                                stats()->read_on_exclusive_miss_for_read_instr_at_l2();
                                if (cache_req->sender == m_id) {
                                    stats()->read_on_exclusive_miss_for_read_instr_at_local_l2();
                                } else {
                                    stats()->read_on_exclusive_miss_for_read_instr_at_remote_l2();
                                }
                            } else {
                                stats()->write_on_exclusive_miss_for_write_instr_at_l2();
                                if (cache_req->sender == m_id) {
                                    stats()->write_on_exclusive_miss_for_write_instr_at_local_l2();
                                } else {
                                    stats()->write_on_exclusive_miss_for_write_instr_at_remote_l2();
                                }
                            }
                        } else {
                            stats()->write_on_shared_miss_for_write_instr_at_l2();
                            if (cache_req->sender == m_id) {
                                stats()->write_on_shared_miss_for_write_instr_at_local_l2();
                            } else {
                                stats()->write_on_shared_miss_for_write_instr_at_remote_l2();
                            }
                        }
                    } else if (per_mem_instr_stats) {
                        per_mem_instr_stats->clear_tentative_data();
                    }

                    entry->status = _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP;
                    entry->substatus = _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__COHERENCE;
                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }

            }

        } else if (entry->status == _DIR_L2_FOR_EMPTY_REQ) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } 
            if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            /* statistics */
            if (per_mem_instr_stats) {
                if (stats_enabled()) {
                    per_mem_instr_stats->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                    if (empty_req->new_requester == m_id) {
                        per_mem_instr_stats->add_local_l2_cost_for_evict(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    } else {
                        per_mem_instr_stats->add_remote_l2_cost_for_evict(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                    }
                    per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                } else {
                    per_mem_instr_stats->clear_tentative_data();
                }
            }

            mh_assert(l2_req->status() == CACHE_REQ_HIT) ;

            if (l2_aux_info->initial_dir_info->dir.size()) {

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] need to invalidate sharers to evict a line of "
                          << start_maddr << endl;

                /* cache the current directory */
                entry->cached_dir = *(l2_aux_info->initial_dir_info);

                /* queue up directory requests to send */
                for (set<uint32_t>::iterator it = entry->cached_dir.dir.begin(); it != entry->cached_dir.dir.end(); ++it) {
                    shared_ptr<coherenceMsg> new_dir_req(new coherenceMsg);
                    new_dir_req->sender = m_id;
                    new_dir_req->receiver = *it;
                    new_dir_req->type = (entry->cached_dir.status == SHARED)? INV_REQ : FLUSH_REQ;
                    new_dir_req->word_count = 0;
                    new_dir_req->maddr = start_maddr;
                    new_dir_req->data = shared_array<uint32_t>();
                    new_dir_req->sent = false;
                    new_dir_req->per_mem_instr_stats = shared_ptr<privateSharedMSIStatsPerMemInstr>();
                    new_dir_req->birthtime = system_time;

                    dir_reqs.push_back(new_dir_req);
                }
                if (!dir_reqs.empty()) {
                    m_dir_req_schedule_q.push_back(entry);
                }

                if (per_mem_instr_stats) {
                    per_mem_instr_stats->begin_inv_for_evict(system_time, entry->cached_dir.dir.size());
                }

                entry->status = _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP;
                entry->substatus = _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__EMPTY_REQ;
                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                /* hit */
                if (stats_enabled()) {
                    stats()->evict_at_l2();
                }

                if (l2_aux_info->is_replaced_line_dirty) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a line of "
                              << start_maddr << " by an empty req without invalidation but with writeback" << endl;
                    if (stats_enabled()) {
                        stats()->writeback_at_l2();
                    }

                    dramctrl_req = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = start_maddr;

                    dramctrl_req->dram_req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                     DRAM_REQ_WRITE,
                                                                                     m_cfg.words_per_cache_line,
                                                                                     l2_aux_info->replaced_line));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false/*local*/, entry));
                    m_dramctrl_writeback_status[start_maddr] = entry;

                    entry->status = _DIR_SEND_DRAMCTRL_WRITEBACK;
                    entry->substatus = _DIR_SEND_DRAMCTRL_WRITEBACK__FROM_EVICT;
                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a line of "
                              << start_maddr << " by an empty req without invalidation/writeback" << endl;
                    empty_req->is_empty_req_done = true;

                    if (entry->using_empty_req_exclusive_space) {
                        ++m_dir_table_vacancy_empty_req_exclusive;
                    } else {
                        ++m_dir_table_vacancy_shared;
                    }
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] vacancy empty req : " 
                              << m_dir_table_vacancy_empty_req_exclusive
                              << " vacancy shared : " << m_dir_table_vacancy_shared << endl;

                    m_dir_table.erase(it_addr++);
                    continue;
                    /* FINISH */
                }
            }

            /* _DIR_L2_FOR_EMPTY_REQ */
        } else if (entry->status == _DIR_L2_FOR_CACHE_REP) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } 
            if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (per_mem_instr_stats) {
                per_mem_instr_stats->clear_tentative_data();
            }

            mh_assert(l2_req->status() == CACHE_REQ_HIT);

            if (entry->using_cache_rep_exclusive_space) {
                ++m_dir_table_vacancy_cache_rep_exclusive;
            } else {
                ++m_dir_table_vacancy_shared;
            }

            m_dir_table.erase(it_addr++);
            continue;
            /* FINISH */

        } else if (entry->status == _DIR_SEND_DRAMCTRL_REQ) {
            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry));
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a DRAM request for "
                      << dramctrl_req->dram_req->maddr() << " to " << dramctrl_req->receiver << endl;
            
            entry->status = _DIR_WAIT_DRAMCTRL_REP;
            ++it_addr;
            continue;
            /* TRANSITION */

        } else if (entry->status == _DIR_WAIT_DRAMCTRL_REP) {

            if (!dramctrl_rep) {
                ++it_addr;
                continue;
                /* SPIN */
            } 

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] received a DRAM reply for " 
                      << dramctrl_rep->dram_req->maddr() << endl;

            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dramctrl_rep_nas(system_time - dramctrl_rep->birthtime);
            }

            shared_ptr<dirCoherenceInfo> new_info(new dirCoherenceInfo);
            new_info->status = (cache_req->type == SH_REQ)? SHARED : EXCLUSIVE;
            new_info->locked = false;
            new_info->dir.insert(cache_req->sender);

            l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line,
                                                               dramctrl_rep->dram_req->read(),
                                                               new_info));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(true);
            l2_req->set_claim(true);
            l2_req->set_evict(true);

            shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence());
            aux_info->core_id = cache_req->sender;
            aux_info->req_type = UPDATE_FOR_DRAMFEED;
            l2_req->set_aux_info_for_coherence(aux_info);

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(entry);
            } else {
                m_l2_write_req_schedule_q.push_back(entry);
            }

            entry->status = _DIR_UPDATE_L2;
            entry->substatus = _DIR_UPDATE_L2__FEED;
            ++it_addr;
            continue;
            /* TRANSITION */

            /* _DIR_WAIT_DRAMCTRL_REP */

        } else if (entry->status == _DIR_UPDATE_L2) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } 
            if (l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (entry->substatus == _DIR_UPDATE_L2__DIR_UPDATE_OR_WRITEBACK) {
                mh_assert(l2_req->status() == CACHE_REQ_HIT);
                
                if (per_mem_instr_stats) {
                    if (stats_enabled()) {
                        per_mem_instr_stats->
                            get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                        if (entry->is_written_back) {
                            if (cache_req->sender == m_id) {
                                per_mem_instr_stats->
                                    add_local_l2_cost_for_directory_update(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            } else {
                                per_mem_instr_stats->
                                    add_remote_l2_cost_for_directory_update(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            }
                        } else {
                            if (cache_req->sender == m_id) {
                                per_mem_instr_stats->
                                    add_local_l2_cost_for_writeback(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            } else {
                                per_mem_instr_stats->
                                    add_remote_l2_cost_for_writeback(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            }
                        }
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                    } else {
                        per_mem_instr_stats->clear_tentative_data();
                    }
                }

                dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                dir_rep->type = (cache_req->type == SH_REQ)? SH_REP : EX_REP;
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;

                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                ++it_addr;
                continue;
                /* TRANSITION */
            }

            if (entry->substatus == _DIR_UPDATE_L2__FEED) {

                if (l2_req->status() == CACHE_REQ_MISS && !l2_victim) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] L2 feed failed due to no victim candidates " << endl;
                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->
                                get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                            if (cache_req->sender == m_id) {
                                per_mem_instr_stats->
                                    add_local_l2_cost_for_feed_retry(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            } else {
                                per_mem_instr_stats->
                                    add_remote_l2_cost_for_feed_retry(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                            }
                            per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                        }
                        stats()->retry_for_update_at_l2();
                    } else if (per_mem_instr_stats) {
                        per_mem_instr_stats->clear_tentative_data();
                    }

                    l2_req->reset();
                    l2_req->set_serialization_begin_time(system_time);
                    if (l2_req->use_read_ports()) {
                        m_l2_read_req_schedule_q.push_back(entry);
                    } else {
                        m_l2_write_req_schedule_q.push_back(entry);
                    }
                    ++it_addr;
                    continue;
                    /* SPIN */
                }

                if (per_mem_instr_stats) {
                    if (stats_enabled()) {
                        per_mem_instr_stats->
                            get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                        if (cache_req->sender == m_id) {
                            per_mem_instr_stats->
                                add_local_l2_cost_for_feed(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        } else {
                            per_mem_instr_stats->
                                add_remote_l2_cost_for_feed(per_mem_instr_stats->get_tentative_data(T_IDX_L2)->total_cost());
                        }
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L2);
                    } else {
                        per_mem_instr_stats->clear_tentative_data();
                    }
                }

                if (l2_req->status() == CACHE_REQ_HIT) {
                    if (!l2_victim || !l2_victim->data_dirty) {
                        /* no need to writeback */

                        if (stats_enabled() && l2_victim) {
                            stats()->evict_at_l2();
                            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] invalidated a line of "
                                      << l2_victim->start_maddr << " by an L2 feed" << endl;
                        }

                        dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                        dir_rep->sender = m_id;
                        dir_rep->receiver = cache_req->sender;
                        dir_rep->type = (cache_req->type == SH_REQ)? SH_REP : EX_REP;
                        dir_rep->word_count = m_cfg.words_per_cache_line;
                        dir_rep->maddr = start_maddr;
                        dir_rep->data = l2_line->data;
                        dir_rep->sent = false;
                        dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                        dir_rep->birthtime = system_time;

                        m_dir_rep_schedule_q.push_back(entry);

                        entry->status = _DIR_SEND_DIR_REP;
                        ++it_addr;
                        continue;
                    } else {
                        /* need to writeback */

                        if (stats_enabled() && l2_victim) {
                            stats()->evict_at_l2();
                            stats()->writeback_at_l2();
                            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] invalidated a line of "
                                      << l2_victim->start_maddr << " by an L2 feed" << endl;
                        }

                        dramctrl_req = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                        dramctrl_req->sender = m_id;
                        dramctrl_req->receiver = m_dramctrl_location;
                        dramctrl_req->maddr = l2_victim->start_maddr;
                        dramctrl_req->dram_req = shared_ptr<dramRequest>(new dramRequest(l2_victim->start_maddr,
                                                                                         DRAM_REQ_WRITE,
                                                                                         m_cfg.words_per_cache_line,
                                                                                         l2_victim->data));
                        dramctrl_req->sent = false;
                        dramctrl_req->birthtime = system_time;
                        dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                        m_dramctrl_req_schedule_q.push_back(make_tuple(false/*local*/, entry));
                        m_dramctrl_writeback_status[l2_victim->start_maddr] = entry;

                        entry->status = _DIR_SEND_DRAMCTRL_WRITEBACK;
                        entry->substatus = _DIR_SEND_DRAMCTRL_WRITEBACK__FROM_L2_FEED;
                        ++it_addr;
                        continue;

                        /* TRANSITION */
                    }

                }  else {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] trying to empty " << l2_victim->start_maddr
                              << " to make space for " << start_maddr << endl;
                    mh_log(4) << "    target's current directory : ";
                    if (l2_victim_info->status == SHARED) {
                        mh_log(4) << "SHARED ";
                    } else {
                        mh_log(4) << "EXCLUSIVE ";
                    }
                    for (set<uint32_t>::iterator it = l2_victim_info->dir.begin(); it != l2_victim_info->dir.end(); ++it) {
                        mh_log(4) << *it << " ";
                    }
                    mh_log(4) << endl;


                    empty_req = shared_ptr<coherenceMsg>(new coherenceMsg);

                    empty_req->sender = m_id;
                    empty_req->receiver = m_id;
                    empty_req->type = EMPTY_REQ;
                    empty_req->word_count = m_cfg.words_per_cache_line;
                    empty_req->maddr = l2_victim->start_maddr;
                    empty_req->data = dramctrl_rep->dram_req->read();
                    empty_req->sent = false;
                    empty_req->replacing_maddr = l2_req->maddr();
                    empty_req->empty_req_for_shreq = (cache_req->type == SH_REQ);
                    empty_req->new_requester = cache_req->sender;
                    empty_req->is_empty_req_done = false;

                    empty_req->per_mem_instr_stats = per_mem_instr_stats;
                    empty_req->birthtime = system_time;

                    m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(false/* local */, empty_req));

                    entry->status = _DIR_EMPTY_VICTIM;
                    ++it_addr;
                    continue;

                }
            }

        } else if (entry->status == _DIR_SEND_DRAMCTRL_WRITEBACK) {
            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry)); /* mark as from local */
                /* need to declare writebacking to prioritize (a separate data structure for simulation performance) */
                m_dramctrl_writeback_status[l2_victim->start_maddr] =  entry;
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (stats_enabled() && per_mem_instr_stats) {
                per_mem_instr_stats->add_dramctrl_req_nas(system_time - dramctrl_req->birthtime);
            }

            if (entry->substatus == _DIR_SEND_DRAMCTRL_WRITEBACK__FROM_L2_FEED) {
                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] written back sent for address "
                          << l2_victim->start_maddr << endl;

                dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                dir_rep->type = (cache_req->type == SH_REQ)? SH_REP : EX_REP;
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;

                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                ++it_addr;
                continue;
            } else {
                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] written back sent for address "
                          << l2_req->maddr() << " for empty req " << endl;

                empty_req->is_empty_req_done = true;

                if (entry->using_empty_req_exclusive_space) {
                    ++m_dir_table_vacancy_empty_req_exclusive;
                } else {
                    ++m_dir_table_vacancy_shared;
                }

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] vacancy empty req : " 
                          << m_dir_table_vacancy_empty_req_exclusive
                          << " vacancy shared : " << m_dir_table_vacancy_shared << endl;

                m_dir_table.erase(it_addr++);
                continue;
                /* FINISH */

            }

        } else if (entry->status == _DIR_EMPTY_VICTIM) {
            if (!empty_req->sent) {
                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] an empty req "
                          << "is trying to get in for address " << empty_req->maddr << endl;

                m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(false/* local */, empty_req));
                ++it_addr;
                continue;
                /* SPIN */
            } 
            if (!empty_req->is_empty_req_done) {
                ++it_addr;
                continue;
                /* SPIN */
            }
            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] finished emptying " << empty_req->maddr 
                      << " for " << start_maddr << endl;
            dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
            dir_rep->sender = m_id;
            dir_rep->receiver = cache_req->sender;
            dir_rep->type = (cache_req->type == SH_REQ)? SH_REP : EX_REP;
            dir_rep->word_count = m_cfg.words_per_cache_line;
            dir_rep->maddr = start_maddr;
            dir_rep->data = dramctrl_rep->dram_req->read();
            dir_rep->sent = false;
            dir_rep->per_mem_instr_stats = per_mem_instr_stats;
            dir_rep->birthtime = system_time;

            m_dir_rep_schedule_q.push_back(entry);

            entry->status = _DIR_SEND_DIR_REP;
            ++it_addr;
            continue;

        } else if (entry->status == _DIR_SEND_DIR_REP) {
            if (!dir_rep->sent) {
                m_dir_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a directory rep (" << dir_rep->type 
                      << ") for " << dir_rep->maddr
                      << " to " << dir_rep->receiver << endl;
            
            ++m_dir_table_vacancy_shared;
            m_dir_table.erase(it_addr++);
            continue;
            /* FINISH */

        } else if (entry->status == _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP) {

            if (cache_rep) {
                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] received a cache reply from " << cache_rep->sender
                          << " for address " << cache_rep->maddr << endl;

                mh_assert(entry->cached_dir.dir.count(cache_rep->sender));

                /* directory speculation */
                if (!m_cfg.use_dir_speculation) {
                    /* must always update the directory when not using speculation */
                    entry->need_to_writeback_dir = true;
                }
                if (entry->substatus == _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__REORDER &&
                    cache_rep->sender != cache_req->sender) 
                {
                    /* didn't expect that other current sharers will send a cache reply. the speculative directory state is not correct, so need to update L2 */
                    entry->need_to_writeback_dir = true;
                }
                if (entry->substatus == _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__COHERENCE &&
                    cache_rep->type == FLUSH_REP)
                {
                    /* expecting a wbRep, but had a flushRep. The speculated directory has the current owner, so need to update this */
                    entry->need_to_writeback_dir = true;
                }

                if (cache_rep->type == WB_REP) {
                    mh_assert(entry->cached_dir.dir.size() == 1);
                    entry->cached_dir.status = SHARED;
                    entry->is_written_back = true;
                    entry->cached_line = cache_rep->data;
                } else {
                    entry->cached_dir.dir.erase(cache_rep->sender);
                    if (cache_rep->type == FLUSH_REP) {
                        entry->is_written_back = true;
                        entry->cached_dir.status = SHARED;
                        entry->cached_line = cache_rep->data;
                    }
                }
                cache_rep = shared_ptr<coherenceMsg>();
            }

            if (entry->substatus == _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__EMPTY_REQ) {
                /* substatus empty req */
                if (entry->cached_dir.dir.size() == 0) {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->end_inv_for_evict(system_time, stats_enabled(), empty_req->new_requester == m_id /* local? */);
                    }
                    if (entry->is_written_back || l2_aux_info->is_replaced_line_dirty) {
                        /* need to writeback */

                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicting a line of "
                                  << start_maddr << " by an empty req and invalidation and with writeback " << endl;
                        if (stats_enabled()) {
                            stats()->evict_at_l2();
                            stats()->writeback_at_l2();
                        }

                        dramctrl_req = shared_ptr<dramctrlMsg>(new dramctrlMsg);
                        dramctrl_req->sender = m_id;
                        dramctrl_req->receiver = m_dramctrl_location;
                        dramctrl_req->maddr = start_maddr;

                        dramctrl_req->dram_req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                         DRAM_REQ_WRITE,
                                                                                         m_cfg.words_per_cache_line,
                                                                                         (entry->is_written_back)? 
                                                                                             entry->cached_line : 
                                                                                             l2_aux_info->replaced_line));
                        dramctrl_req->sent = false;
                        dramctrl_req->birthtime = system_time;
                        dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                        m_dramctrl_req_schedule_q.push_back(make_tuple(false/*local*/, entry));
                        m_dramctrl_writeback_status[start_maddr] = entry;

                        entry->status = _DIR_SEND_DRAMCTRL_WRITEBACK;
                        entry->substatus = _DIR_SEND_DRAMCTRL_WRITEBACK__FROM_EVICT;
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else {
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicting a line of "
                                  << start_maddr << " by an empty req and invalidation and without writeback " << endl;
                        if (stats_enabled()) {
                            stats()->evict_at_l2();
                        }

                        empty_req->is_empty_req_done = true;

                        if (entry->using_empty_req_exclusive_space) {
                            ++m_dir_table_vacancy_empty_req_exclusive;
                        } else {
                            ++m_dir_table_vacancy_shared;
                        }

                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] vacancy empty req : " 
                                  << m_dir_table_vacancy_empty_req_exclusive
                                  << " vacancy shared : " << m_dir_table_vacancy_shared << endl;

                        m_dir_table.erase(it_addr++);
                        continue;
                        /* FINISH */
                    }
                }
            } else {

                bool is_done = false;

                if (entry->substatus == _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__REORDER) {
                    if (entry->cached_dir.dir.count(cache_req->sender) == 0) {
                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->end_reorder(system_time, stats_enabled(), cache_req->sender == m_id /*local?*/);
                        }
                        mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] reordering finished " << endl;

                        is_done = true;

                    }
                } else {
                    /* coherence */
                    if ((cache_req->type == SH_REQ && entry->cached_dir.status == SHARED) ||
                        (cache_req->type == EX_REQ && entry->cached_dir.dir.empty()))
                    {
                        /* invalidation is finished */
                        if (per_mem_instr_stats) {
                            per_mem_instr_stats->end_inv_for_coherence(system_time, stats_enabled(), cache_req->sender == m_id /*local?*/);
                        }
                        mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] invalidation (for coherence) finished on " << start_maddr << endl;

                        is_done = true;

                    }
                }

                if (is_done) {
                    if (entry->is_written_back || entry->need_to_writeback_dir) {

                        shared_ptr<dirCoherenceInfo> new_info(new dirCoherenceInfo(entry->cached_dir));
                        l2_req = 
                            shared_ptr<cacheRequest>(new cacheRequest(start_maddr, 
                                                                      CACHE_REQ_UPDATE,
                                                                      (entry->is_written_back)? m_cfg.words_per_cache_line : 0,
                                                                      entry->cached_line,
                                                                      (entry->need_to_writeback_dir)? 
                                                                      new_info : shared_ptr<dirCoherenceInfo>())
                                                    );
                        l2_req->set_serialization_begin_time(system_time);
                        l2_req->set_unset_dirty_on_write(!entry->is_written_back);
                        l2_req->set_claim(false);
                        l2_req->set_evict(false);

                        shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence());
                        aux_info->core_id = cache_req->sender;
                        aux_info->req_type = UPDATE_FOR_DIR_UPDATE_OR_WRITEBACK;
                        l2_req->set_aux_info_for_coherence(aux_info);

                        if (l2_req->use_read_ports()) {
                            m_l2_read_req_schedule_q.push_back(entry);
                        } else {
                            m_l2_write_req_schedule_q.push_back(entry);
                        }

                        entry->status = _DIR_UPDATE_L2;
                        entry->substatus = _DIR_UPDATE_L2__DIR_UPDATE_OR_WRITEBACK;
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else {
                        dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                        dir_rep->sender = m_id;
                        dir_rep->receiver = cache_req->sender;
                        dir_rep->type = (cache_req->type == SH_REQ)? SH_REP : EX_REP;
                        dir_rep->word_count = m_cfg.words_per_cache_line;
                        dir_rep->maddr = start_maddr;
                        dir_rep->data = l2_line->data;
                        dir_rep->sent = false;
                        dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                        dir_rep->birthtime = system_time;

                        m_dir_rep_schedule_q.push_back(entry);

                        entry->status = _DIR_SEND_DIR_REP;
                        ++it_addr;
                        continue;
                        /* TRANSITION */
                    }
                }
            }

            if (!dir_reqs.empty()) {
                if (dir_reqs.front()->sent) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a directory request to " 
                              << dir_reqs.front()->receiver 
                              << " for address " << dir_reqs.front()->maddr << endl;
                    dir_reqs.erase(dir_reqs.begin());
                }
                if (!dir_reqs.empty()) {
                    m_dir_req_schedule_q.push_back(entry);
                }
            }

            ++it_addr;
            continue;
            /* SPIN */

        }
    }

}

void privateSharedMSI::update_dramctrl_work_table() {

    for (dramctrlTable::iterator it_addr = m_dramctrl_work_table.begin(); it_addr != m_dramctrl_work_table.end(); ) {
        shared_ptr<dramctrlTableEntry>& entry = it_addr->second;
        shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
        shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;
        shared_ptr<privateSharedMSIStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        /* only reads are in the table */
        if (dramctrl_req->dram_req->status() == DRAM_REQ_DONE) {

            if (!dramctrl_rep) {
                if (stats_enabled() && per_mem_instr_stats) {
                    per_mem_instr_stats->add_dram_ops(system_time - entry->operation_begin_time);
                }
                dramctrl_rep = shared_ptr<dramctrlMsg>(new dramctrlMsg);
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

void privateSharedMSI::accept_incoming_messages() {

    /* Directory requests and replies (from the network) */
    while (m_core_receive_queues[MSG_DIR_REQ_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DIR_REQ_REP]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        if (data_msg->type == SH_REP || data_msg->type == EX_REP) {
            maddr_t msg_start_maddr = get_start_maddr_in_line(data_msg->maddr);
            mh_assert(m_cache_table.count(msg_start_maddr) &&
                      m_cache_table[msg_start_maddr]->status == _CACHE_WAIT_DIR_REP);
            m_cache_table[msg_start_maddr]->dir_rep = data_msg;
            m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
        } else {
            // dir req 
            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] a directory req from " << data_msg->sender 
                      << " is trying to get in for address " << data_msg->maddr << endl;
            m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_REMOTE_DIR_REQ, data_msg));
        }
        /* only one message a time (otherwise out-of-order reception may happen) */
        break;
    }

    /* Cache requests (from the network) */
    while (m_core_receive_queues[MSG_CACHE_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_REQ]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(true/* is remote */, data_msg));
        mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] a cache req from " << data_msg->sender 
                  << " is trying to get in for address " << data_msg->maddr << endl;
        break;
    }
        
    /* Cache replies (from the network) */
    while (m_core_receive_queues[MSG_CACHE_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_REP]->front();
        shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_dir_table_entry_for_cache_rep_schedule_q.push_back(make_tuple(true/* is remote */, data_msg));
        mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] a cache rep from " << data_msg->sender 
                  << " is trying to get in for address " << data_msg->maddr << endl;
        break;
    }

    /* DRAMCTRL requests and replies from the network */
    if (m_core_receive_queues[MSG_DRAMCTRL_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REQ]->front();
        shared_ptr<dramctrlMsg> dram_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        m_dramctrl_req_schedule_q.push_back(make_tuple(true/* is remote */, dram_msg));
    }

    if (m_core_receive_queues[MSG_DRAMCTRL_REP]->size()) {
        /* note: no replies for DRAM writes */
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REP]->front();
        shared_ptr<dramctrlMsg> dramctrl_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        maddr_t start_maddr = dramctrl_msg->dram_req->maddr(); /* always access by a cache line */
        mh_assert(m_dir_table.count(start_maddr) > 0 &&
                  m_dir_table[dramctrl_msg->maddr]->status == _DIR_WAIT_DRAMCTRL_REP);
        m_dir_table[start_maddr]->dramctrl_rep = dramctrl_msg;
        m_core_receive_queues[MSG_DRAMCTRL_REP]->pop();
    }

}

void privateSharedMSI::schedule_requests() {

    static boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    /*****************************************/
    /* scheduling for sending cache requests */
    /*****************************************/

    random_shuffle(m_cache_req_schedule_q.begin(), m_cache_req_schedule_q.end(), rr_fn);
    while (m_cache_req_schedule_q.size()) {
        shared_ptr<cacheTableEntry>& entry = m_cache_req_schedule_q.front();
        shared_ptr<coherenceMsg>& cache_req = entry->cache_req;

        if (m_id == cache_req->receiver) {
            m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(false, cache_req));
        } else if (m_core_send_queues[MSG_CACHE_REQ]->available()) {
            shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = cache_req->receiver;
            msg->type = MSG_CACHE_REQ;
            msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
            msg->content = cache_req;
            m_core_send_queues[MSG_CACHE_REQ]->push_back(msg);
            cache_req->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cache request sent " << m_id << " -> " 
                      << msg->dst << " num flits " << msg->flit_count << endl;

        }

        m_cache_req_schedule_q.erase(m_cache_req_schedule_q.begin());
    }

    /****************************************/
    /* scheduling for sending cache replies */
    /****************************************/

    random_shuffle(m_cache_rep_schedule_q.begin(), m_cache_rep_schedule_q.end(), rr_fn);
    while (m_cache_rep_schedule_q.size()) {
        shared_ptr<cacheTableEntry>& entry = m_cache_rep_schedule_q.front();
        shared_ptr<coherenceMsg>& cache_rep = entry->cache_rep;

        if (m_id == cache_rep->receiver) {
            m_new_dir_table_entry_for_cache_rep_schedule_q.push_back(make_tuple(false, cache_rep));
        } else if (m_core_send_queues[MSG_CACHE_REP]->available()) {
            shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = cache_rep->receiver;
            msg->type = MSG_CACHE_REP;
            msg->flit_count = get_flit_count(1 + cache_rep->word_count * 4);
            msg->content = cache_rep;
            m_core_send_queues[MSG_CACHE_REP]->push_back(msg);
            cache_rep->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cache reply sent " << m_id << " -> " 
                      << msg->dst << " num flits " << msg->flit_count << endl;

        }

        m_cache_rep_schedule_q.erase(m_cache_rep_schedule_q.begin());

    }

    /*********************************************/
    /* scheduling for sending directory requests */
    /*********************************************/

    random_shuffle(m_dir_req_schedule_q.begin(), m_dir_req_schedule_q.end(), rr_fn);
    while (m_dir_req_schedule_q.size()) {
        shared_ptr<dirTableEntry>& entry = m_dir_req_schedule_q.front();
        shared_ptr<coherenceMsg>& dir_req = entry->dir_reqs.front();

        if (m_id == dir_req->receiver) {
            m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_LOCAL_DIR_REQ, dir_req));
        } else if (m_core_send_queues[MSG_DIR_REQ_REP]->available()) {
            shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = dir_req->receiver;
            msg->type = MSG_DIR_REQ_REP;
            msg->flit_count = get_flit_count(1 + m_cfg.words_per_cache_line * 4);
            msg->content = dir_req;
            m_core_send_queues[MSG_DIR_REQ_REP]->push_back(msg);
            dir_req->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] directory request sent " << m_id << " -> " 
                      << msg->dst << " num flits " << msg->flit_count << endl;
        }

        m_dir_req_schedule_q.erase(m_dir_req_schedule_q.begin());
    }

    /********************************************/
    /* scheduling for sending directory replies */
    /********************************************/

    random_shuffle(m_dir_rep_schedule_q.begin(), m_dir_rep_schedule_q.end(), rr_fn);
    while (m_dir_rep_schedule_q.size()) {
        shared_ptr<dirTableEntry>& entry = m_dir_rep_schedule_q.front();
        shared_ptr<coherenceMsg>& dir_rep = entry->dir_rep;

        if (m_id == dir_rep->receiver) {
            mh_assert(m_cache_table.count(dir_rep->maddr));
            mh_assert(m_cache_table[dir_rep->maddr]->status == _CACHE_WAIT_DIR_REP);
            m_cache_table[dir_rep->maddr]->dir_rep = dir_rep;
            dir_rep->sent = true;

            if (stats_enabled()) {
                if (dir_rep->type == SH_REP) {
                    stats()->shrep_sent(m_id != dir_rep->receiver);
                } else {
                    stats()->exrep_sent(m_id != dir_rep->receiver);
                }
            }

        } else if (m_core_send_queues[MSG_DIR_REQ_REP]->available()) {
            shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = dir_rep->receiver;
            msg->type = MSG_DIR_REQ_REP;
            msg->flit_count = get_flit_count(1 + dir_rep->word_count * 4);
            msg->content = dir_rep;
            m_core_send_queues[MSG_DIR_REQ_REP]->push_back(msg);
            dir_rep->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] directory reply sent " << m_id << " -> " 
                      << msg->dst << " num flits " << msg->flit_count << endl;

            if (stats_enabled()) {
                if (dir_rep->type == SH_REP) {
                    stats()->shrep_sent(m_id != dir_rep->receiver);
                } else {
                    stats()->exrep_sent(m_id != dir_rep->receiver);
                }
            }

        }

        m_dir_rep_schedule_q.erase(m_dir_rep_schedule_q.begin());

    }

    /*****************************/
    /* scheduling for core ports */
    /*****************************/

    random_shuffle(m_core_port_schedule_q.begin(), m_core_port_schedule_q.end(), rr_fn);
    uint32_t requested = 0;
    while (m_core_port_schedule_q.size()) {
        shared_ptr<memoryRequest> req = m_core_port_schedule_q.front();
        if (requested < m_available_core_ports) {
            m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_LOCAL_CORE_REQ, req));
            ++requested;
        } else {
            set_req_status(req, REQ_RETRY);
        }
        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
    }

    /**********************************/
    /* schedule for cache table space */
    /**********************************/

    random_shuffle(m_new_cache_table_entry_schedule_q.begin(), m_new_cache_table_entry_schedule_q.end(), rr_fn);
    while (m_new_cache_table_entry_schedule_q.size()) {
        cacheTableEntrySrc_t source = m_new_cache_table_entry_schedule_q.front().get<0>();
        if (source == FROM_LOCAL_CORE_REQ) {

            shared_ptr<memoryRequest> core_req = static_pointer_cast<memoryRequest>(m_new_cache_table_entry_schedule_q.front().get<1>());
            shared_ptr<privateSharedMSIStatsPerMemInstr> per_mem_instr_stats = 
                (core_req->per_mem_instr_runtime_info())?
                    static_pointer_cast<privateSharedMSIStatsPerMemInstr>(*(core_req->per_mem_instr_runtime_info()))
                :
                    shared_ptr<privateSharedMSIStatsPerMemInstr>();
            maddr_t start_maddr = get_start_maddr_in_line(core_req->maddr());
            if (m_cache_table.count(start_maddr) || m_cache_table_vacancy == 0 || m_available_core_ports == 0) {
                set_req_status(core_req, REQ_RETRY);
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;
            }
            if (stats_enabled()) {
                if (per_mem_instr_stats) {
                    per_mem_instr_stats->add_mem_srz(system_time - per_mem_instr_stats->serialization_begin_time_at_current_core());
                }
                if (core_req->is_read()) {
                    stats()->new_read_instr_at_l1();
                } else {
                    stats()->new_write_instr_at_l1();
                }
            }

            shared_ptr<catRequest> cat_req(new catRequest(core_req->maddr(), m_id));
            cat_req->set_serialization_begin_time(system_time);

            shared_ptr<cacheRequest> l1_req(new cacheRequest(core_req->maddr(),
                                                             core_req->is_read()? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                                             core_req->word_count(),
                                                             core_req->is_read()? shared_array<uint32_t>() : core_req->data()));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(false);
            l1_req->set_claim(false);
            l1_req->set_evict(false);
            l1_req->set_aux_info_for_coherence
                (shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(core_req->is_read()? LOCAL_READ : LOCAL_WRITE)));

            shared_ptr<cacheTableEntry> new_entry(new cacheTableEntry);
            new_entry->core_req =  core_req;
            new_entry->cat_req = cat_req;
            new_entry->l1_req = l1_req;
            new_entry->dir_req = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->cache_rep = shared_ptr<coherenceMsg>();

            new_entry->per_mem_instr_stats = per_mem_instr_stats;

            new_entry->status = _CACHE_CAT_AND_L1_FOR_LOCAL;

            m_cat_req_schedule_q.push_back(new_entry);
            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l1_write_req_schedule_q.push_back(new_entry);
            }

            m_cache_table[start_maddr] = new_entry;
            --m_cache_table_vacancy;

            --m_available_core_ports;

            m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());

            mh_log(4) << "[Mem " << m_id << " @ " << system_time 
                      << " ] A core request on " << core_req->maddr() << " got into the table " << endl;

        } else {

            /* a directory request from the local or a remote directory */

            shared_ptr<coherenceMsg> dir_req = static_pointer_cast<coherenceMsg>(m_new_cache_table_entry_schedule_q.front().get<1>());
            mh_assert(get_start_maddr_in_line(dir_req->maddr) == dir_req->maddr);
            if (m_cache_table.count(dir_req->maddr)) {
                
                /* A directory request must not block a directory reply for which the existing entry created by a local request */
                /* on the same cache line is waiting. */
                /* This could happen when a cache line is evicted from cache before a directory request arrives, */
                /* and a following local access on the cache line gets a miss and sends a cache request. */
                /* Therefore, if the existing entry gets a miss, then the directory request can and must be discarded. */
                /* If the entry's state is not _CACHE_CAT_AND_L1_FOR_LOCAL, it gets a miss. */
                /* And if it didn't receive a directory reply yet, it IS waiting for a directory reply that comes after */
                /* the current directory request */

                if ((m_cache_table[dir_req->maddr]->status != _CACHE_CAT_AND_L1_FOR_LOCAL) &&
                    !(m_cache_table[dir_req->maddr]->dir_rep))
                {
                    /* consumed */
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] discarded a directory request (" << dir_req->type
                              << ") for address " << dir_req->maddr << " fron " << dir_req->sender
                              << " (state: " << m_cache_table[dir_req->maddr]->status 
                              << " ) " << endl;
                    if (source == FROM_REMOTE_DIR_REQ) {
                        m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                    } else {
                        dir_req->sent = true;
                    }
                    
                    if (stats_enabled()) {
                        if (dir_req->type == INV_REQ) {
                            stats()->invreq_sent(m_id != dir_req->receiver);
                        } else if (dir_req->type == FLUSH_REQ) {
                            stats()->flushreq_sent(m_id != dir_req->receiver);
                        } else if (dir_req->type == WB_REQ) {
                            stats()->wbreq_sent(m_id != dir_req->receiver);
                        }
                    }

                }

                /* If the existing entry is testing the L1, or if it alrady received a directory reply and is trying to */
                /* update its L1, the directory request must not be discarded and taken after the current entry finishes */
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;

            } else if (m_cache_table_vacancy == 0) {
                //mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] a directory request on " << dir_req->maddr
                //          << " could not get info the table for the table is full." << endl;
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;
            }

            mh_log(4) << "[L1 " << m_id  << " @ " << system_time << " ] received a directory request (" << dir_req->type 
                      << ") for address " << dir_req->maddr << endl;

            if (stats_enabled()) {
                if (dir_req->type == INV_REQ) {
                    stats()->invreq_sent(m_id != dir_req->receiver);
                } else if (dir_req->type == FLUSH_REQ) {
                    stats()->flushreq_sent(m_id != dir_req->receiver);
                } else if (dir_req->type == WB_REQ) {
                    stats()->wbreq_sent(m_id != dir_req->receiver);
                }
            }

            shared_ptr<cacheRequest> l1_req;
            if (dir_req->type == WB_REQ) {
                shared_ptr<cacheCoherenceInfo> new_info(new cacheCoherenceInfo);
                new_info->status = SHARED;
                new_info->home = dir_req->sender;
                l1_req = shared_ptr<cacheRequest>(new cacheRequest(dir_req->maddr, CACHE_REQ_UPDATE,
                                                                   0, shared_array<uint32_t>(), new_info));
                l1_req->set_aux_info_for_coherence
                    (shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(UPDATE_BY_WBREQ)));
            } else {
                /* invReq or flushReq */
                l1_req = shared_ptr<cacheRequest>(new cacheRequest(dir_req->maddr, CACHE_REQ_INVALIDATE));
                if (dir_req->type == INV_REQ) {
                    l1_req->set_aux_info_for_coherence
                        (shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(INVALIDATE_BY_INVREQ)));
                } else {
                    l1_req->set_aux_info_for_coherence
                        (shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(INVALIDATE_BY_FLUSHREQ)));
                }
            }
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(false);
            l1_req->set_claim(false);
            l1_req->set_evict(false);

            shared_ptr<cacheTableEntry> new_entry(new cacheTableEntry);
            new_entry->core_req =  shared_ptr<memoryRequest>();
            new_entry->cat_req = shared_ptr<catRequest>();
            new_entry->l1_req = l1_req;
            new_entry->dir_req = dir_req;
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->cache_rep = shared_ptr<coherenceMsg>();

            new_entry->per_mem_instr_stats = shared_ptr<privateSharedMSIStatsPerMemInstr>();

            new_entry->status = _CACHE_L1_FOR_DIR_REQ;

            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l1_write_req_schedule_q.push_back(new_entry);
            }

            m_cache_table[dir_req->maddr] = new_entry;
            --m_cache_table_vacancy;

            if (source == FROM_LOCAL_DIR_REQ) {
                dir_req->sent = true;
            } else {
                m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
            }
            m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
            continue;

        }
    }

    /* cache replies is prior to other requests */
    random_shuffle(m_new_dir_table_entry_for_cache_rep_schedule_q.begin(), m_new_dir_table_entry_for_cache_rep_schedule_q.end(), rr_fn);
    while (m_new_dir_table_entry_for_cache_rep_schedule_q.size()) {
        bool is_remote = m_new_dir_table_entry_for_cache_rep_schedule_q.front().get<0>();
        shared_ptr<coherenceMsg> cache_rep = m_new_dir_table_entry_for_cache_rep_schedule_q.front().get<1>();
        mh_assert(get_start_maddr_in_line(cache_rep->maddr) == cache_rep->maddr);
        if (m_dir_table.count(cache_rep->maddr)) {
            if (m_dir_table[cache_rep->maddr]->status == _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP &&
                !m_dir_table[cache_rep->maddr]->cache_rep) 
            {
                m_dir_table[cache_rep->maddr]->cache_rep = cache_rep;
                if (is_remote) {
                    m_core_receive_queues[MSG_CACHE_REP]->pop();
                } else {
                    cache_rep->sent = true;
                }
                if (stats_enabled()) {
                    if (cache_rep->type == INV_REP) {
                        stats()->invrep_sent(is_remote);
                    } else if (cache_rep->type == FLUSH_REP) {
                        stats()->flushrep_sent(is_remote);
                    } else {
                        stats()->wbrep_sent(is_remote);
                    }
                }

            }
            m_new_dir_table_entry_for_cache_rep_schedule_q.erase(m_new_dir_table_entry_for_cache_rep_schedule_q.begin());
            continue;
        } else if (m_dir_table_vacancy_cache_rep_exclusive == 0 && m_dir_table_vacancy_shared == 0)  {
            m_new_dir_table_entry_for_cache_rep_schedule_q.erase(m_new_dir_table_entry_for_cache_rep_schedule_q.begin());
            continue;
        } else {
            bool use_exclusive = m_dir_table_vacancy_cache_rep_exclusive > 0;

            if (stats_enabled()) {
                if (cache_rep->type == INV_REP) {
                    stats()->invrep_sent(is_remote);
                } else if (cache_rep->type == FLUSH_REP) {
                    stats()->flushrep_sent(is_remote);
                } else {
                    stats()->wbrep_sent(is_remote);
                }
            }

            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache reply from " << cache_rep->sender 
                      << " for address " << cache_rep->maddr << " (new entry) " << endl;

            shared_ptr<cacheRequest> l2_req;
            l2_req = shared_ptr<cacheRequest>(new cacheRequest(cache_rep->maddr, CACHE_REQ_UPDATE,
                                                               (cache_rep->type == INV_REP)? 0 : m_cfg.words_per_cache_line,
                                                               (cache_rep->type == INV_REP)? shared_array<uint32_t>() : cache_rep->data
                                                              ));
            l2_req->set_serialization_begin_time(UINT64_MAX);
            l2_req->set_unset_dirty_on_write(false);
            l2_req->set_claim(false);
            l2_req->set_evict(false);

            shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence());
            aux_info->core_id = cache_rep->sender;
            if (cache_rep->type == WB_REP) {
                aux_info->req_type = UPDATE_FOR_WBREP;
            } else {
                aux_info->req_type = UPDATE_FOR_INVREP_OR_FLUSHREP;
            }
            l2_req->set_aux_info_for_coherence(aux_info);

            shared_ptr<dirTableEntry> new_entry(new dirTableEntry);
            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->l2_req = l2_req;
            new_entry->cache_rep = cache_rep;
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->dramctrl_req = shared_ptr<dramctrlMsg>();
            new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
            new_entry->empty_req = shared_ptr<coherenceMsg>();
            new_entry->is_written_back = false;
            new_entry->need_to_writeback_dir = false;
            if (use_exclusive) {
                --m_dir_table_vacancy_cache_rep_exclusive;
                new_entry->using_cache_rep_exclusive_space = true;
                new_entry->using_empty_req_exclusive_space = false;
            } else {
                --m_dir_table_vacancy_shared;
                new_entry->using_cache_rep_exclusive_space = false;
                new_entry->using_empty_req_exclusive_space = false;
            }
            new_entry->per_mem_instr_stats = cache_rep->per_mem_instr_stats;

            new_entry->status = _DIR_L2_FOR_CACHE_REP;

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l2_write_req_schedule_q.push_back(new_entry);
            }

            m_dir_table[cache_rep->maddr] = new_entry;

            if (is_remote) {
                m_core_receive_queues[MSG_CACHE_REP]->pop();
            } else {
                cache_rep->sent = true;
            }

            m_new_dir_table_entry_for_cache_rep_schedule_q.erase(m_new_dir_table_entry_for_cache_rep_schedule_q.begin());
            continue;
            
        }
                
    }

    /* cache requests/empty requests scheuling */
    random_shuffle(m_new_dir_table_entry_for_req_schedule_q.begin(), m_new_dir_table_entry_for_req_schedule_q.end(), rr_fn);
    while (m_new_dir_table_entry_for_req_schedule_q.size()) {
        bool is_remote = m_new_dir_table_entry_for_req_schedule_q.front().get<0>();
        shared_ptr<coherenceMsg> req = static_pointer_cast<coherenceMsg>(m_new_dir_table_entry_for_req_schedule_q.front().get<1>());

        mh_assert(get_start_maddr_in_line(req->maddr) == req->maddr);

        if (m_dir_table.count(req->maddr)) {
            /* need to finish the previous entry first */
            m_new_dir_table_entry_for_req_schedule_q.erase(m_new_dir_table_entry_for_req_schedule_q.begin());
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache request (" << req->type 
                      << ") from " << req->sender << " for address " << req->maddr << " but has an existing entry " << endl;
            continue;
        }

        bool use_exclusive = (req->type == EMPTY_REQ && m_dir_table_vacancy_empty_req_exclusive > 0);
        if (m_dir_table_vacancy_shared == 0 && !use_exclusive) {
            /* no space */
            m_new_dir_table_entry_for_req_schedule_q.erase(m_new_dir_table_entry_for_req_schedule_q.begin());
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache request (" << req->type 
                      << ") from " << req->sender << " for address " << req->maddr << " but has no space " << endl;
            continue;
        }

        if (is_remote) {
            m_core_receive_queues[MSG_CACHE_REQ]->pop();
        } else {
            req->sent = true;
        }
            
        mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache request (" << req->type 
                  << ") from " << req->sender << " for address " << req->maddr << " (new entry) " << endl;

        shared_ptr<cacheRequest> l2_req;
        shared_ptr<dirTableEntry> new_entry(new dirTableEntry);

        if (req->type == EMPTY_REQ) {

            if (stats_enabled()) {
                stats()->empty_req_sent();
            }

            shared_ptr<dirCoherenceInfo> new_info(new dirCoherenceInfo);
            new_info->status = (req->empty_req_for_shreq)? SHARED : EXCLUSIVE;
            new_info->locked = false;
            new_info->dir.insert(req->new_requester);

            l2_req = shared_ptr<cacheRequest>(new cacheRequest(req->maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line, req->data, new_info));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(req->empty_req_for_shreq);
            l2_req->set_claim(false);
            l2_req->set_evict(false);

            shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence());
            aux_info->core_id = req->new_requester;
            aux_info->req_type = UPDATE_FOR_EMPTYREQ;
            aux_info->replacing_maddr = req->replacing_maddr;
            aux_info->empty_req_for_shreq = req->empty_req_for_shreq;
            aux_info->is_replaced_line_dirty = false;
            l2_req->set_aux_info_for_coherence(aux_info);

            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->empty_req = req;

            new_entry->status = _DIR_L2_FOR_EMPTY_REQ;
            new_entry->per_mem_instr_stats = req->per_mem_instr_stats;

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l2_write_req_schedule_q.push_back(new_entry);
            }

            if (stats_enabled() && req->per_mem_instr_stats) {
                req->per_mem_instr_stats->add_empty_req_srz(system_time - req->birthtime, req->new_requester == m_id/*local?*/);
            }

        } else {

            if (stats_enabled()) {
                if (req->type == SH_REQ) {
                    stats()->shreq_sent(is_remote);
                } else {
                    stats()->exreq_sent(is_remote);
                }
            }

            l2_req = shared_ptr<cacheRequest>(new cacheRequest(req->maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(false);
            l2_req->set_claim(false);
            l2_req->set_evict(false);

            shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence());
            aux_info->core_id = req->sender;
            aux_info->req_type = (req->type == SH_REQ)? READ_FOR_SHREQ : READ_FOR_EXREQ;
            l2_req->set_aux_info_for_coherence(aux_info);

            new_entry->cache_req = req;
            new_entry->empty_req = shared_ptr<coherenceMsg>();

            new_entry->status = _DIR_L2_FOR_CACHE_REQ;
            new_entry->per_mem_instr_stats = req->per_mem_instr_stats;

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l2_write_req_schedule_q.push_back(new_entry);
            }

            if (stats_enabled()) {
                if (req->per_mem_instr_stats) {
                    req->per_mem_instr_stats->add_cache_req_nas(system_time - req->birthtime);
                }
                if (req->type == SH_REQ) {
                    stats()->new_read_instr_at_l2();
                } else {
                    stats()->new_write_instr_at_l2();
                }
            }

        }

        new_entry->l2_req = l2_req;
        new_entry->cache_rep = shared_ptr<coherenceMsg>();
        new_entry->dir_rep = shared_ptr<coherenceMsg>();
        new_entry->dramctrl_req = shared_ptr<dramctrlMsg>();
        new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
        new_entry->is_written_back = false;
        new_entry->need_to_writeback_dir = false;

        if (use_exclusive) {
            --m_dir_table_vacancy_empty_req_exclusive;
            new_entry->using_cache_rep_exclusive_space = false;
            new_entry->using_empty_req_exclusive_space = true;
        } else {
            --m_dir_table_vacancy_shared;
            new_entry->using_cache_rep_exclusive_space = false;
            new_entry->using_empty_req_exclusive_space = false;
        }

        if (l2_req->use_read_ports()) {
            m_l2_read_req_schedule_q.push_back(new_entry);
        } else {
            m_l2_write_req_schedule_q.push_back(new_entry);
        }

        m_dir_table[req->maddr] = new_entry;

        m_new_dir_table_entry_for_req_schedule_q.erase(m_new_dir_table_entry_for_req_schedule_q.begin());
        continue;

    }

    /************************************/
    /* scheduling for dramctrl requests */
    /************************************/
    
    random_shuffle(m_dramctrl_req_schedule_q.begin(), m_dramctrl_req_schedule_q.end(), rr_fn);
    while (m_dramctrl_req_schedule_q.size()) {
        bool is_remote = m_dramctrl_req_schedule_q.front().get<0>();
        shared_ptr<dirTableEntry> entry = shared_ptr<dirTableEntry>();
        shared_ptr<dramctrlMsg> dramctrl_msg = shared_ptr<dramctrlMsg>();
        if (is_remote) {
            dramctrl_msg = static_pointer_cast<dramctrlMsg>(m_dramctrl_req_schedule_q.front().get<1>());
        } else {
            entry = static_pointer_cast<dirTableEntry>(m_dramctrl_req_schedule_q.front().get<1>());
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
                    shared_ptr<dramctrlTableEntry> new_entry(new dramctrlTableEntry);
                    new_entry->dramctrl_req = dramctrl_msg;
                    new_entry->dramctrl_rep = shared_ptr<dramctrlMsg>();
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

                shared_ptr<message_t> msg(new message_t);
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
        shared_ptr<cacheTableEntry> entry = m_cat_req_schedule_q.front();
        shared_ptr<catRequest> cat_req = entry->cat_req;

        if (cat_req->serialization_begin_time() == UINT64_MAX) {
            cat_req->set_operation_begin_time(UINT64_MAX);
        } else {
            cat_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_CAT)->add_cat_srz
                    (system_time - cat_req->serialization_begin_time());
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
        shared_ptr<cacheTableEntry>& entry = m_l1_read_req_schedule_q.front();
        shared_ptr<cacheRequest>& l1_req = entry->l1_req;

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
        shared_ptr<cacheTableEntry>& entry = m_l1_write_req_schedule_q.front();
        shared_ptr<cacheRequest>& l1_req = entry->l1_req;

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
        shared_ptr<dirTableEntry>& entry = m_l2_read_req_schedule_q.front();
        shared_ptr<cacheRequest>& l2_req = entry->l2_req;

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
        shared_ptr<dirTableEntry>& entry = m_l2_write_req_schedule_q.front();
        shared_ptr<cacheRequest>& l2_req = entry->l2_req;

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

    /*******************************************/
    /* scheduling for sending dramctrl replies */
    /*******************************************/

    random_shuffle(m_dramctrl_rep_schedule_q.begin(), m_dramctrl_rep_schedule_q.end(), rr_fn);
    while (m_dramctrl_rep_schedule_q.size()) {
        shared_ptr<dramctrlTableEntry>& entry = m_dramctrl_rep_schedule_q.front();
        shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
        shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;

        if (dramctrl_req->sender == m_id) {
            mh_assert(m_dir_table.count(dramctrl_req->maddr));
            m_dir_table[dramctrl_req->maddr]->dramctrl_rep = entry->dramctrl_rep;
            mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] has sent a DRAMCTRL rep for address " 
                      << dramctrl_rep->maddr << " to core " << m_id << endl;
            dramctrl_rep->sent = true;

        } else if (m_core_send_queues[MSG_DRAMCTRL_REP]->available()) {

            shared_ptr<message_t> msg(new message_t);
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

