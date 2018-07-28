// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedPTI.hpp"
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
#define T_IDX_DIR 2
#define T_IDX_L2 3

/***********************************************************/
/* Cache helper functions to customize the cache behaviors */
/***********************************************************/

/* coherence info copier */
static std::shared_ptr<void> cache_copy_coherence_info_ideal(std::shared_ptr<void> source) {
        std::shared_ptr<privateSharedPTI::cacheCoherenceInfo> ret
        (new privateSharedPTI::cacheCoherenceInfo(*static_pointer_cast<privateSharedPTI::cacheCoherenceInfo>(source)));
    /* pointer of timestamp is copied */
    return ret;
}
static std::shared_ptr<void> cache_copy_coherence_info(std::shared_ptr<void> source) {
        std::shared_ptr<privateSharedPTI::cacheCoherenceInfo> ret
        (new privateSharedPTI::cacheCoherenceInfo(*static_pointer_cast<privateSharedPTI::cacheCoherenceInfo>(source)));
    /* copy of timestamp value is passed */
    ret->timestamp = std::shared_ptr<uint64_t>(new uint64_t(*ret->timestamp));
    return ret;
}

static std::shared_ptr<void> dir_copy_coherence_info_ideal(std::shared_ptr<void> source) {
        std::shared_ptr<privateSharedPTI::dirCoherenceInfo> ret
        (new privateSharedPTI::dirCoherenceInfo(*static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(source)));
    /* pointer of timestamp is copied */
    return ret;
}
static std::shared_ptr<void> dir_copy_coherence_info(std::shared_ptr<void> source) {
        std::shared_ptr<privateSharedPTI::dirCoherenceInfo> ret
        (new privateSharedPTI::dirCoherenceInfo(*static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(source)));
    /* copy of timestamp value is passed */
    ret->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(*ret->max_timestamp));
    return ret;
}

/* hit checkers */
static bool cache_hit_checker(std::shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) {

        std::shared_ptr<privateSharedPTI::cacheCoherenceInfo> cache_coherence_info = 
        static_pointer_cast<privateSharedPTI::cacheCoherenceInfo>(line.coherence_info);
        std::shared_ptr<privateSharedPTI::cacheAuxInfoForCoherence> request_info = 
        static_pointer_cast<privateSharedPTI::cacheAuxInfoForCoherence>(req->aux_info_for_coherence());

    if (*request_info == privateSharedPTI::LOCAL_READ) {
        if (cache_coherence_info->status == privateSharedPTI::TIMESTAMPED && 
            *(cache_coherence_info->timestamp) < system_time )
        {
            /* read miss on expired T */
            return false;
        }
    } else if (*request_info == privateSharedPTI::LOCAL_WRITE) {
        if (cache_coherence_info->status == privateSharedPTI::TIMESTAMPED) {
            /* write miss on T mode */
            return false;
        }
    }
    return true;
}

static bool dir_hit_checker(std::shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) {

    /* do both test checks and directory updates */

        std::shared_ptr<privateSharedPTI::dirCoherenceInfo> info = 
        static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(line.coherence_info);
        std::shared_ptr<privateSharedPTI::dirAuxInfoForCoherence> request_info = 
        static_pointer_cast<privateSharedPTI::dirAuxInfoForCoherence>(req->aux_info_for_coherence());
    const privateSharedPTI::privateSharedPTICfg_t& cfg = request_info->cfg;

    if (cfg.renewal_type == privateSharedPTI::_RENEWAL_IDEAL) {
        request_info->previous_info = 
            static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(dir_copy_coherence_info_ideal(info));
    } else {
        request_info->previous_info = 
            static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(dir_copy_coherence_info(info));
    }

    if (request_info->req_type == privateSharedPTI::READ_FOR_TREQ) {

        bool hit = true;
        if (info->status == privateSharedPTI::PRIVATE) {
            hit = false;
        } 
        /* speculation */
        info->status = privateSharedPTI::TIMESTAMPED;
        if (cfg.renewal_type == privateSharedPTI::_RENEWAL_IDEAL) {
            mh_assert(*(info->max_timestamp)==UINT64_MAX);
        } else {
            *(info->max_timestamp) = system_time + cfg.delta;
        }
        return hit;

    } else if (request_info->req_type == privateSharedPTI::READ_FOR_PREQ) {

        bool hit;
        if (info->status == privateSharedPTI::TIMESTAMPED) {
            if (*(info->max_timestamp) < system_time) {
                hit = true;
            } else {
                if (cfg.renewal_type == privateSharedPTI::_RENEWAL_IDEAL) {
                    *(request_info->previous_info->max_timestamp) = system_time;
                    info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
                    hit = true;
                } else {
                    hit = false;
                }
            }
        } else {
            /* either owned by another core, or reordered */
            hit = false;
        }
        /* speculation */
        info->status = privateSharedPTI::PRIVATE;
        info->owner = request_info->core_id;

        return hit;

    } else if (request_info->req_type == privateSharedPTI::READ_FOR_RREQ) {
        mh_assert(cfg.renewal_type != privateSharedPTI::_RENEWAL_IDEAL &&
                  cfg.renewal_type != privateSharedPTI::_RENEWAL_NEVER);
        if (info->status == privateSharedPTI::TIMESTAMPED) {
            if (cfg.rRep_type == privateSharedPTI::_NEVER) {
                if (cfg.allow_revive && request_info->requester_timestamp > info->last_write_time) {
                    info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(system_time + cfg.delta));
                    return true;
                } else if (!cfg.allow_revive && request_info->requester_timestamp > system_time) {
                    info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(system_time + cfg.delta));
                    return true;
                } else {
                    return false;
                }
            } else if (cfg.rRep_type == privateSharedPTI::_ALWAYS) {
                info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(system_time + cfg.delta));
                request_info->need_to_send_block = true;
                return true;
            } else if (cfg.rRep_type == privateSharedPTI::_SELECTIVE) {
                if (cfg.allow_revive) {
                    info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(system_time + cfg.delta));
                    if (request_info->requester_timestamp > info->last_write_time) {
                        request_info->need_to_send_block = true;
                    }
                    return true;
                } else {
                    if (request_info->requester_timestamp > system_time) {
                        info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(system_time + cfg.delta));
                        return true;
                    } else {
                        return false;
                    }
                }
            }
        } else {
            return false;
        }

    } else if (request_info->req_type == privateSharedPTI::UPDATE_FOR_EMPTY_REQ) {

        mh_assert(info->locked);

        if (line.data_dirty) {
            request_info->is_replaced_line_dirty = true;
            request_info->replaced_line = shared_array<uint32_t>(new uint32_t[req->word_count()]);
            for (uint32_t i = 0; i < req->word_count(); ++i) {
                request_info->replaced_line[i] = line.data[i];
            }
        }
        line.start_maddr = request_info->replacing_maddr;
        if (cfg.renewal_type == privateSharedPTI::_RENEWAL_IDEAL) {
            *(request_info->previous_info->max_timestamp) = system_time;
        }
        return true;
    } else if (request_info->req_type == privateSharedPTI::UPDATE_FOR_CACHE_REP_FOR_REQ) {
        if (cfg.renewal_type == privateSharedPTI::_RENEWAL_IDEAL) {
            info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
        } else {
            if (info->status == privateSharedPTI::TIMESTAMPED) {
                *(info->max_timestamp) = system_time + cfg.delta; 
            }
        }
    } else if (request_info->req_type == privateSharedPTI::UPDATE_FOR_CACHE_REP_ON_NEW) {
        info->status = privateSharedPTI::TIMESTAMPED;
        if (cfg.renewal_type == privateSharedPTI::_RENEWAL_IDEAL) {
            info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
        } else {
            *(info->max_timestamp) = system_time; 
        }
    }

    return true;

}

static void dir_update_hook(std::shared_ptr<cacheRequest> req, cacheLine &line, const uint64_t& system_time) {

        std::shared_ptr<privateSharedPTI::dirCoherenceInfo> info = 
        static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(line.coherence_info);
        std::shared_ptr<privateSharedPTI::dirAuxInfoForCoherence> request_info = 
        static_pointer_cast<privateSharedPTI::dirAuxInfoForCoherence>(req->aux_info_for_coherence());
    const privateSharedPTI::privateSharedPTICfg_t& cfg = request_info->cfg;

    if (request_info->req_type == privateSharedPTI::UPDATE_FOR_FEED || 
        request_info->req_type == privateSharedPTI::UPDATE_FOR_EMPTY_REQ) 
    {
        info->max_timestamp = std::shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
        if (cfg.renewal_type != privateSharedPTI::_RENEWAL_IDEAL) {
            *(info->max_timestamp) = system_time + cfg.delta;
        }
    }
    if (req->word_count()) {
        info->last_write_time = system_time;
    }
}

/* evictable checker */

static bool dir_can_evict_line(cacheLine &line, const uint64_t& system_time) {
        std::shared_ptr<privateSharedPTI::dirCoherenceInfo> info = 
        static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(line.coherence_info);
    if (info->locked) 
        return false;
    return true;
}

static bool dir_evict_need_action(cacheLine &line, const uint64_t& system_time) {
        std::shared_ptr<privateSharedPTI::dirCoherenceInfo> info = 
        static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(line.coherence_info);
    if (info->status == privateSharedPTI::PRIVATE || *(info->max_timestamp) > system_time) {
        info->locked = true;
        return true;
    } else {
        return false;
    }
}

static bool dir_evict_need_action_ideal(cacheLine &line, const uint64_t& system_time) {
        std::shared_ptr<privateSharedPTI::dirCoherenceInfo> info = 
        static_pointer_cast<privateSharedPTI::dirCoherenceInfo>(line.coherence_info);
    if (info->status == privateSharedPTI::PRIVATE) {
        info->locked = true;
        return true;
    } else {
        if (*(info->max_timestamp) > system_time) {
            *(info->max_timestamp) = system_time;
        }
        return false;
    }
}
privateSharedPTI::privateSharedPTI(uint32_t id,
                                   const uint64_t &t,
                                   std::shared_ptr<tile_statistics> st,
                                   logger &l,
                                   std::shared_ptr<random_gen> r,
                                   std::shared_ptr<cat> a_cat,
                                   privateSharedPTICfg_t cfg) :
    memory(id, t, st, l, r),
    m_cfg(cfg),
    m_l1(NULL),
    m_l2(NULL),
    m_cat(a_cat),
    m_stats(std::shared_ptr<privateSharedPTIStatsPerTile>()),
    m_cache_table_vacancy(cfg.cache_table_size),
    m_dir_table_vacancy_shared(cfg.dir_table_size_shared),
    m_dir_table_vacancy_cache_rep_exclusive(cfg.dir_table_size_cache_rep_exclusive),
    m_dir_table_vacancy_empty_req_exclusive(cfg.dir_table_size_empty_req_exclusive),
    m_cache_renewal_table_vacancy(m_cfg.cache_renewal_table_size),
    m_dir_renewal_table_vacancy(m_cfg.dir_renewal_table_size),
    m_available_core_ports(cfg.num_local_core_ports),
    m_rReq_schedule_q(m_cfg.retry_rReq, t)
{
    /* sanity checks */
    if (m_cfg.bytes_per_flit == 0) throw err_bad_shmem_cfg("flit size must be non-zero.");
    if (m_cfg.words_per_cache_line == 0) throw err_bad_shmem_cfg("cache line size must be non-zero.");
    if (m_cfg.lines_in_l1 == 0) throw err_bad_shmem_cfg("privateSharedPTI : L1 size must be non-zero.");
    if (m_cfg.lines_in_l2 == 0) throw err_bad_shmem_cfg("privateSharedPTI : L2 size must be non-zero.");
    if (m_cfg.cache_table_size == 0) 
        throw err_bad_shmem_cfg("privateSharedPTI : cache table size must be non-zero.");
    if (m_cfg.dir_table_size_shared == 0) 
        throw err_bad_shmem_cfg("privateSharedPTI : shared directory table size must be non-zero.");
    if (m_cfg.dir_table_size_cache_rep_exclusive == 0) 
        throw err_bad_shmem_cfg("privateSharedPTI : cache reply exclusive work table size must be non-zero.");
    if (m_cfg.dir_table_size_empty_req_exclusive == 0) 
        throw err_bad_shmem_cfg("privateSharedPTI : empty request exclusive work table size must be non-zero.");
    if ((m_cfg.renewal_type == _RENEWAL_SYNCHED ||
         m_cfg.renewal_type == _RENEWAL_SCHEDULED) &&
        (m_cfg.cache_renewal_table_size == 0 ||
         m_cfg.dir_renewal_table_size == 0))
    {
        throw err_bad_shmem_cfg("privateSharedPTI : renewal table size must be nonzero for cache and difrectory for renewal.");
    }
    
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

    if (m_cfg.renewal_type == _RENEWAL_IDEAL) {
        m_l1->set_helper_copy_coherence_info(&cache_copy_coherence_info_ideal);
        m_l2->set_helper_copy_coherence_info(&dir_copy_coherence_info_ideal);
        m_l2->set_helper_evict_need_action(&dir_evict_need_action_ideal);
    } else {
        m_l1->set_helper_copy_coherence_info(&cache_copy_coherence_info);
        m_l2->set_helper_copy_coherence_info(&dir_copy_coherence_info);
        m_l2->set_helper_evict_need_action(&dir_evict_need_action);
    }


    m_l1->set_helper_is_coherence_hit(&cache_hit_checker);
    m_l2->set_helper_is_coherence_hit(&dir_hit_checker);

    m_l2->set_helper_can_evict_line(&dir_can_evict_line);
    m_l2->set_update_hook(&dir_update_hook);

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

privateSharedPTI::~privateSharedPTI() {
    delete m_l1;
    delete m_l2;
}

uint32_t privateSharedPTI::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

void privateSharedPTI::request(std::shared_ptr<memoryRequest> req) {

    /* assumes a request is not across multiple cache lines */
    uint32_t __attribute__((unused)) byte_offset = req->maddr().address%(m_cfg.words_per_cache_line*4);
    mh_assert( (byte_offset + req->word_count()*4) <= m_cfg.words_per_cache_line * 4);

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    /* per memory instruction info */
    std::shared_ptr<std::shared_ptr<void> > p_runtime_info = req->per_mem_instr_runtime_info();
    std::shared_ptr<void>& runtime_info = *p_runtime_info;
    std::shared_ptr<privateSharedPTIStatsPerMemInstr> per_mem_instr_stats;
    if (!runtime_info) {
        /* no per-instr stats: this is the first time this memory instruction is issued */
        per_mem_instr_stats = std::shared_ptr<privateSharedPTIStatsPerMemInstr>(new privateSharedPTIStatsPerMemInstr(req->is_read()));
        per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
        runtime_info = per_mem_instr_stats;
    } else {
        per_mem_instr_stats = 
            static_pointer_cast<privateSharedPTIStatsPerMemInstr>(*req->per_mem_instr_runtime_info());
        if (per_mem_instr_stats->is_in_migration()) {
            per_mem_instr_stats = static_pointer_cast<privateSharedPTIStatsPerMemInstr>(runtime_info);
            per_mem_instr_stats->migration_finished(system_time, stats_enabled());
            per_mem_instr_stats->set_serialization_begin_time_at_current_core(system_time);
        }
    }

    /* will schedule for a core port and a work table entry in schedule function */
    m_core_port_schedule_q.push_back(req);

}

void privateSharedPTI::tick_positive_edge() {
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

void privateSharedPTI::tick_negative_edge() {

    m_l1->tick_negative_edge();
    m_l2->tick_negative_edge();
    m_cat->tick_negative_edge();
    if(m_dramctrl) {
        m_dramctrl->tick_negative_edge();
    }

    /* accept messages and write into tables */
    accept_incoming_messages();

    update_dir_table();
    update_cache_table();
    update_dramctrl_work_table();

}

uint32_t privateSharedPTI::get_header_bytes(coherenceMsgType_t type) {
    uint32_t bytes = 1 + m_cfg.address_size_in_bytes;
    switch (type) {
    case TREQ:
    case RREQ:
        bytes += m_cfg.address_size_in_bytes;
        /* need to send the current expiration time to the directory */
        bytes += 8; 
        break;
    case SWITCH_REQ:
    case TREP:
    case RREP:
        bytes += 8; /* 64-bit timestamp */
    default:
        break;
    }
    return bytes;
}

void privateSharedPTI::schedule_requests() {

    static boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    /*****************************************/
    /* scheduling for sending cache requests */
    /*****************************************/

    random_shuffle(m_cache_req_schedule_q.begin(), m_cache_req_schedule_q.end(), rr_fn);
    while (m_power && m_cache_req_schedule_q.size()) {
            std::shared_ptr<cacheTableEntry>& entry = m_cache_req_schedule_q.front();
            std::shared_ptr<coherenceMsg>& cache_req = entry->cache_req;

        if (m_id == cache_req->receiver) {
            if (cache_req->type == RREQ) {
                m_new_dir_table_entry_for_renewal_schedule_q.push_back(make_tuple(FROM_LOCAL_CACHE, cache_req));
            } else {
                m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(FROM_LOCAL_CACHE, cache_req));
            }
        } else if (m_core_send_queues[MSG_CACHE_REQ]->available()) {
                std::shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = cache_req->receiver;
            if (m_cfg.use_exclusive_vc_for_pReq && cache_req->type == PREQ) {
                msg->type = MSG_CACHE_PREQ;
            } else if (m_cfg.use_exclusive_vc_for_rReq && cache_req->type == RREQ) {
                msg->type = MSG_CACHE_RREQ;
            } else {
                msg->type = MSG_CACHE_REQ;
            }
            msg->flit_count = get_flit_count(get_header_bytes(cache_req->type));
            msg->content = cache_req;
            m_core_send_queues[msg->type]->push_back(msg);
            cache_req->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] ";
            if (cache_req->type == TREQ) {
                mh_log(4) << "tReq ";
            } else if (cache_req->type == PREQ) {
                mh_log(4) << "pReq ";
            } else {
                mh_log(4) << "rReq ";
            }
            mh_log(4) << "sent " << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;
        }

        m_cache_req_schedule_q.erase(m_cache_req_schedule_q.begin());
    }

    /****************************************/
    /* scheduling for sending cache replies */
    /****************************************/

    random_shuffle(m_cache_rep_schedule_q.begin(), m_cache_rep_schedule_q.end(), rr_fn);
    while (m_cache_rep_schedule_q.size()) {
            std::shared_ptr<cacheTableEntry>& entry = m_cache_rep_schedule_q.front();
            std::shared_ptr<coherenceMsg>& cache_rep = entry->cache_rep;

        if (m_id == cache_rep->receiver) {
            m_new_dir_table_entry_for_cache_rep_schedule_q.push_back(make_tuple(false, cache_rep));
        } else if (m_core_send_queues[MSG_CACHE_REP]->available()) {
                std::shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = cache_rep->receiver;
            msg->type = MSG_CACHE_REP;
            msg->flit_count = get_flit_count(get_header_bytes(cache_rep->type) + cache_rep->word_count * 4);
            msg->content = cache_rep;
            m_core_send_queues[MSG_CACHE_REP]->push_back(msg);
            cache_rep->sent = true;

            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] ";
            if (cache_rep->type == SWITCH_REP) {
                mh_log(4) << "switchRep ";
            } else {
                mh_log(4) << "invRep ";
            }
            mh_log(4) << " sent " << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;

        }

        m_cache_rep_schedule_q.erase(m_cache_rep_schedule_q.begin());

    }

    /*********************************************/
    /* scheduling for sending directory requests */
    /*********************************************/
    random_shuffle(m_dir_req_schedule_q.begin(), m_dir_req_schedule_q.end(), rr_fn);
    while (m_dir_req_schedule_q.size()) {
            std::shared_ptr<dirTableEntry>& entry = m_dir_req_schedule_q.front();
            std::shared_ptr<coherenceMsg>& dir_req = entry->dir_req;
        if (m_id == dir_req->receiver) {
            m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_LOCAL_DIR, dir_req));
        } else if (m_core_send_queues[MSG_DIR_REQ_REP]->available()) {
                std::shared_ptr<message_t> msg(new message_t);
            msg->src = m_id;
            msg->dst = dir_req->receiver;
            msg->type = MSG_DIR_REQ_REP;
            msg->flit_count = get_flit_count(get_header_bytes(dir_req->type) + m_cfg.words_per_cache_line * 4);
            msg->content = dir_req;
            m_core_send_queues[MSG_DIR_REQ_REP]->push_back(msg);
            dir_req->sent = true;
            mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] ";
            if (dir_req->type == INV_REQ) {
                mh_log(4) << "invReq ";
            } else if (dir_req->type == SWITCH_REQ) {
                mh_log(4) << "switchReq ";
            }
            mh_log(4) << "sent " << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;
        }
        m_dir_req_schedule_q.erase(m_dir_req_schedule_q.begin());
    }

    /********************************************/
    /* scheduling for sending directory replies */
    /********************************************/
    random_shuffle(m_dir_rep_schedule_q.begin(), m_dir_rep_schedule_q.end(), rr_fn);
    while (m_dir_rep_schedule_q.size()) {
            std::shared_ptr<dirTableEntry>& entry = m_dir_rep_schedule_q.front();
            std::shared_ptr<coherenceMsg>& dir_rep = entry->dir_rep;
        if (m_id == dir_rep->receiver) {
            m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_LOCAL_DIR, dir_rep));
        } else {
            privateSharedPTIMsgType_t msg_type;
            if (m_cfg.use_exclusive_vc_for_rRep && dir_rep->type == RREP) {
                msg_type = MSG_DIR_RREP;
            } else {
                msg_type = MSG_DIR_REQ_REP;
            }
            if (m_core_send_queues[msg_type]->available()) {
                    std::shared_ptr<message_t> msg(new message_t);
                msg->src = m_id;
                msg->dst = dir_rep->receiver;
                msg->type = msg_type;
                msg->flit_count = get_flit_count(get_header_bytes(dir_rep->type) + dir_rep->word_count * 4);
                msg->content = dir_rep;
                m_core_send_queues[msg_type]->push_back(msg);
                dir_rep->sent = true;

                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] ";
                if (dir_rep->type == TREP) {
                    mh_log(4) << "tRep ";
                } else if (dir_rep->type == PREP) {
                    mh_log(4) << "pRep ";
                } else if (dir_rep->type == RREP) {
                    mh_log(4) << "rRep ";
                }
                mh_log(4) << " sent " << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;
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
            std::shared_ptr<memoryRequest> req = m_core_port_schedule_q.front();
        if (requested < m_available_core_ports) {
            m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_LOCAL_CORE_REQ, req));
            ++requested;
        } else {
            set_req_status(req, REQ_RETRY);
        }
        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
    }

    /*********************************/
    /* scheduling for scheduled rReq */
    /*********************************/
    if (m_cfg.renewal_type == _RENEWAL_SCHEDULED) {
        vector<std::shared_ptr<coherenceMsg> > on_due = m_rReq_schedule_q.on_due();
        while (on_due.size()) {
            m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_LOCAL_SCHEDULED_RREQ, on_due.front()));
            on_due.erase(on_due.begin());
        }
    }

    /**********************************/
    /* schedule for cache table space */
    /**********************************/
    random_shuffle(m_new_cache_table_entry_schedule_q.begin(), m_new_cache_table_entry_schedule_q.end(), rr_fn);
    while (m_new_cache_table_entry_schedule_q.size()) {
        cacheTableEntrySrc_t source = get<0>(m_new_cache_table_entry_schedule_q.front());
        if (source == FROM_LOCAL_CORE_REQ) {
            /* Requests from core */
                std::shared_ptr<memoryRequest> core_req = 
                static_pointer_cast<memoryRequest>(get<1>(m_new_cache_table_entry_schedule_q.front()));
                std::shared_ptr<privateSharedPTIStatsPerMemInstr> per_mem_instr_stats = 
                (core_req->per_mem_instr_runtime_info())?
                    static_pointer_cast<privateSharedPTIStatsPerMemInstr>(*(core_req->per_mem_instr_runtime_info()))
                :
                    std::shared_ptr<privateSharedPTIStatsPerMemInstr>();
            maddr_t start_maddr = get_start_maddr_in_line(core_req->maddr());

            bool retry = false;
            if (m_cache_table_vacancy == 0 || m_available_core_ports == 0) {
                retry = true;
            } else if (m_cache_table.count(start_maddr)) {
                if (m_cache_table[start_maddr]->status == _CACHE_SEND_RREQ) {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] " 
                              << "halted sending rReq process for a new core request" << endl;
                    ++m_cache_renewal_table_vacancy;
                } else {
                    retry = true;
                }
            }
            if (retry) {
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

            /* Create a new entry */
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
            l1_req->set_aux_info_for_coherence
                (std::shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(core_req->is_read() ? LOCAL_READ : LOCAL_WRITE)));

            std::shared_ptr<cacheTableEntry> new_entry(new cacheTableEntry);
            new_entry->core_req =  core_req;
            new_entry->cat_req = cat_req;
            new_entry->l1_req = l1_req;
            new_entry->dir_req = std::shared_ptr<coherenceMsg>();
            new_entry->dir_rep = std::shared_ptr<coherenceMsg>();
            new_entry->cache_req = std::shared_ptr<coherenceMsg>();
            new_entry->cache_rep = std::shared_ptr<coherenceMsg>();
            new_entry->status = core_req->is_read()? _CACHE_CAT_AND_L1_FOR_READ : _CACHE_CAT_AND_L1_FOR_WRITE;
            new_entry->per_mem_instr_stats = per_mem_instr_stats;

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

            mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] A core ";
            if (core_req->is_read()) {
                mh_log(4) << "read ";
            } else {
                mh_log(4) << "write ";
            }
            mh_log(4) << "request on " << core_req->maddr() << " got into the table " << endl;

        } else if (source == FROM_LOCAL_SCHEDULED_RREQ) {

                std::shared_ptr<coherenceMsg> rReq = static_pointer_cast<coherenceMsg>(get<1>(m_new_cache_table_entry_schedule_q.front()));

            bool discard = false;
            bool retry = false;

            if (!m_cfg.allow_revive && *(rReq->timestamp) <= system_time) {
                discard = true;
            } else if (m_cache_renewal_table_vacancy == 0) {
                retry = true;
            } else if (m_cache_table.count(rReq->maddr)) {
                discard = true;
            }

            if (discard) {
                m_rReq_schedule_q.remove(rReq->maddr);
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;
            } else if (retry) {
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;
            }

            m_rReq_schedule_q.remove(rReq->maddr);

            std::shared_ptr<cacheTableEntry> new_entry(new cacheTableEntry);
            new_entry->core_req =  std::shared_ptr<memoryRequest>();
            new_entry->cat_req = std::shared_ptr<catRequest>();
            new_entry->l1_req = std::shared_ptr<cacheRequest>();
            new_entry->dir_req = std::shared_ptr<coherenceMsg>();
            new_entry->dir_rep = std::shared_ptr<coherenceMsg>();
            new_entry->cache_req = rReq;
            new_entry->cache_rep = std::shared_ptr<coherenceMsg>();
            new_entry->status = _CACHE_SEND_RREQ;
            new_entry->per_mem_instr_stats = std::shared_ptr<privateSharedPTIStatsPerMemInstr>();

            m_cache_table[rReq->maddr] = new_entry;
            --m_cache_renewal_table_vacancy;

            m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());

            mh_log(4) << "[Mem " << m_id << " @ " << system_time << " ] An rReq "
                      << "request on " << rReq->maddr << " got into the table " << endl;

        } else {
            /* from directory */
                std::shared_ptr<coherenceMsg> dir_msg = static_pointer_cast<coherenceMsg>(get<1>(m_new_cache_table_entry_schedule_q.front()));
            maddr_t start_maddr = get_start_maddr_in_line(dir_msg->maddr);

            if (dir_msg->type == INV_REQ || dir_msg->type == SWITCH_REQ) {
                mh_assert(start_maddr == dir_msg->maddr);

                bool discard = false;
                bool retry = false;

                if (m_cache_table.count(dir_msg->maddr)) {
                    if (m_cache_table[dir_msg->maddr]->dir_rep) {
                        retry = true;
                    } else if (m_cache_table[dir_msg->maddr]->status == _CACHE_SEND_RREQ) {
                        if (m_cache_table_vacancy == 0) {
                            retry = true;
                        } else {
                            ++m_cache_renewal_table_vacancy;
                            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] " 
                                      << "halted sending rReq process for a new core request" << endl;
                        }
                    } else if (m_cache_table[dir_msg->maddr]->status != _CACHE_CAT_AND_L1_FOR_READ &&
                               m_cache_table[dir_msg->maddr]->status != _CACHE_CAT_AND_L1_FOR_WRITE &&
                               m_cache_table[dir_msg->maddr]->status != _CACHE_SEND_RREQ)
                    {
                        /* A directory request must not block a directory reply for which the existing entry created by a local request */
                        /* on the same cache line is waiting. */
                        /* This could happen when a cache line is evicted from cache before a directory request arrives, */
                        /* and a following local access on the cache line gets a miss and sends a cache request. */
                        /* Therefore, if the existing entry gets a miss, then the directory request can and must be discarded. */
                        /* If the entry's state is not _CACHE_CAT_AND_L1_FOR_LOCAL, or sending an rReq, it gets a miss. */
                        /* And if it didn't receive a directory reply yet, it IS waiting for a directory reply that comes after */
                        /* the current directory request */
                        discard = true;
                    } else {
                        retry = true;
                    }
                } else if (m_cache_table_vacancy == 0) {
                    retry = true;
                }

                if (discard) {
                    /* consumed */
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] discarded a ";
                    if (dir_msg->type == SWITCH_REQ) {
                        mh_log(4) << "switchReq ";
                    } else if (dir_msg->type == INV_REQ){
                        mh_log(4) << "invReq ";
                    }
                    mh_log(4) << " for address " << dir_msg->maddr << " from " << dir_msg->sender
                        << " (state: " << m_cache_table[dir_msg->maddr]->status 
                        << " ) " << endl;
                    if (source == FROM_REMOTE_DIR) {
                        m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                    } else if (source == FROM_REMOTE_DIR_RREP) {
                        m_core_receive_queues[MSG_DIR_RREP]->pop();
                    } else {
                        dir_msg->sent = true;
                    }
                    m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                    continue;
                } else if (retry) {
                    m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                    continue;
                }

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a ";
                if (dir_msg->type == SWITCH_REQ) {
                    mh_log(4) << "switchReq (TIMESTAMP: " << *(dir_msg->timestamp) << " ) ";
                } else if (dir_msg->type == INV_REQ){
                    mh_log(4) << "invReq ";
                }
                mh_log(4) << "for address " << dir_msg->maddr << " from " << dir_msg->sender << endl;

                /* Create a new entry */
                std::shared_ptr<cacheRequest> l1_req;
                if (dir_msg->type == SWITCH_REQ) {
                        std::shared_ptr<cacheCoherenceInfo> new_info(new cacheCoherenceInfo);
                    new_info->status = TIMESTAMPED;
                    new_info->home = dir_msg->sender;
                    new_info->timestamp = dir_msg->timestamp;

                    l1_req = std::shared_ptr<cacheRequest>(new cacheRequest(dir_msg->maddr, CACHE_REQ_UPDATE,
                                                                       0, shared_array<uint32_t>(), new_info));
                    l1_req->set_aux_info_for_coherence
                        (std::shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(UPDATE_FOR_SWITCH_REQ)));
                } else {
                    mh_assert(dir_msg->type == INV_REQ); /* invReq */
                    l1_req = std::shared_ptr<cacheRequest>(new cacheRequest(dir_msg->maddr, CACHE_REQ_INVALIDATE));
                    l1_req->set_aux_info_for_coherence
                        (std::shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(INVALIDATE_FOR_INV_REQ)));
                }
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(false); 
                l1_req->set_claim(false);
                l1_req->set_evict(false);

                std::shared_ptr<cacheTableEntry> new_entry(new cacheTableEntry);
                new_entry->core_req =  std::shared_ptr<memoryRequest>();
                new_entry->cat_req = std::shared_ptr<catRequest>();
                new_entry->l1_req = l1_req;
                new_entry->dir_req = dir_msg;
                new_entry->dir_rep = std::shared_ptr<coherenceMsg>();
                new_entry->cache_req = std::shared_ptr<coherenceMsg>();
                new_entry->cache_rep = std::shared_ptr<coherenceMsg>();
                new_entry->per_mem_instr_stats = std::shared_ptr<privateSharedPTIStatsPerMemInstr>();
                new_entry->status = _CACHE_L1_FOR_DIR_REQ;

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(new_entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(new_entry);
                }

                m_cache_table[dir_msg->maddr] = new_entry;
                --m_cache_table_vacancy;

                if (source == FROM_REMOTE_DIR) {
                    m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                } else if (source == FROM_REMOTE_DIR_RREP) {
                    m_core_receive_queues[MSG_DIR_RREP]->pop();
                } else {
                    dir_msg->sent = true;
                }
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;

            } else if (dir_msg->type == PREP) {
                /* PREP */
                mh_assert(m_cache_table.count(start_maddr) &&
                          (m_cache_table[start_maddr]->status == _CACHE_WAIT_PREP ||
                           m_cache_table[start_maddr]->status == _CACHE_WAIT_TREP_OR_RREP));
                m_cache_table[start_maddr]->dir_rep = dir_msg;
                if (source == FROM_REMOTE_DIR) {
                    m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                } else if (source == FROM_REMOTE_DIR_RREP) {
                    m_core_receive_queues[MSG_DIR_RREP]->pop();
                } else {
                    dir_msg->sent = true;
                }
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;

            } else if (!m_cache_table.count(start_maddr)) {

                /* TREP or RREP, as a renewal */
                bool discard = false;
                bool retry = false;

                if (*(dir_msg->timestamp) <= system_time) {
                    /* cannot use rRep with expired timestamp */
                    discard = true;
                    mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] an expired ";
                    if (dir_msg->type == TREP) {
                        mh_log(4) << "tRep ";
                    } else {
                        mh_assert(dir_msg->type == RREP);
                        mh_log(4) << "rRep ";
                    }
                    mh_log(4) << "from " << dir_msg->sender << " on " << dir_msg->maddr << " is discarded" << endl;
                } else if (m_cache_renewal_table_vacancy == 0) {
                    /* no space for renewal */
                    retry = true;
                }

                if (discard) {
                    if (source == FROM_REMOTE_DIR) {
                        m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                    } else if (source == FROM_REMOTE_DIR_RREP) {
                        m_core_receive_queues[MSG_DIR_RREP]->pop();
                    } else {
                        dir_msg->sent = true;
                    }
                    m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                    continue;
                } else if (retry) {
                    m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                    continue;
                }

                /* accepted as a renewal */


                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a ";
                if (dir_msg->type == TREP) {
                    mh_log(4) << "tRep ";
                } else if (dir_msg->type == RREP){
                    mh_log(4) << "rRep ";
                }
                mh_log(4) << "as a renewal for address " << dir_msg->maddr << " from " << dir_msg->sender << endl;

                /* Create a new entry */
                std::shared_ptr<cacheRequest> l1_req;
                std::shared_ptr<cacheCoherenceInfo> new_info(new cacheCoherenceInfo);
                new_info->status = TIMESTAMPED;
                new_info->home = dir_msg->sender;
                new_info->timestamp = dir_msg->timestamp;
                l1_req = std::shared_ptr<cacheRequest>(new cacheRequest(dir_msg->maddr, CACHE_REQ_UPDATE,
                                                                   dir_msg->word_count,
                                                                   dir_msg->data, 
                                                                   new_info));
                l1_req->set_aux_info_for_coherence
                    (std::shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(UPDATE_FOR_RENEWAL)));
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(true);
                l1_req->set_claim(false);
                l1_req->set_evict(false);
                std::shared_ptr<cacheTableEntry> new_entry(new cacheTableEntry);
                new_entry->core_req =  std::shared_ptr<memoryRequest>();
                new_entry->cat_req = std::shared_ptr<catRequest>();
                new_entry->l1_req = l1_req;
                new_entry->dir_req = dir_msg;
                new_entry->dir_rep = std::shared_ptr<coherenceMsg>();
                new_entry->cache_req = std::shared_ptr<coherenceMsg>();
                new_entry->cache_rep = std::shared_ptr<coherenceMsg>();
                new_entry->per_mem_instr_stats = std::shared_ptr<privateSharedPTIStatsPerMemInstr>();
                new_entry->status = _CACHE_L1_FOR_RENEWAL;

                --m_cache_renewal_table_vacancy;
                m_cache_table[dir_msg->maddr] = new_entry;

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q_for_renewal.push_back(new_entry);
                } else {
                    m_l1_write_req_schedule_q_for_renewal.push_back(new_entry);
                }
                if (source == FROM_REMOTE_DIR) {
                    m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                } else if (source == FROM_REMOTE_DIR_RREP) {
                    m_core_receive_queues[MSG_DIR_RREP]->pop();
                } else {
                    dir_msg->sent = true;
                }
                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;

            } else {
                /* TREP or RREP, as a reply */

                bool discard = false;
                bool retry = false;
                if (dir_msg->type == RREP && *(dir_msg->timestamp) <= system_time) {
                    /* cannot use rRep with expired timestamp */
                    discard = true;
                    mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] an expired rRep from " << dir_msg->sender 
                              << " on " << dir_msg->maddr << " is discarded" << endl;
                } else if (m_cache_table[start_maddr]->status == _CACHE_WAIT_PREP) {
                    /* pReq already sent. tRep and rRep must get discarded */
                    discard = true;
                    mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] waiting for a pRep and discarded a ";
                    if (dir_msg->type == TREP) {
                        mh_log(4) << "tRep ";
                    } else {
                        mh_log(4) << "rRep ";
                    }
                    mh_log(4) << "from " << dir_msg->sender << " on " << dir_msg->maddr << endl;
                } else if (!m_cfg.use_rRep_for_tReq) {
                    if (dir_msg->type == RREP) {
                        discard = true;
                        mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] waiting for a tRep and discarded a rRep"
                                  << "from " << dir_msg->sender << " on " << dir_msg->maddr << endl;
                    } else {
                        mh_assert(m_cache_table[start_maddr]->status == _CACHE_WAIT_TREP_OR_RREP);
                    }
                } else {
                    if (*(dir_msg->timestamp) <= system_time && 
                        m_cache_table[start_maddr]->cache_req->requested_time != dir_msg->requested_time)
                    {
                        /* invalid tRep (tRep for previous tReq, that is answered early by a rRep */
                        mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] discarded an obsolete tRep from "
                                  << dir_msg->sender << " on " << dir_msg->maddr << endl;
                        discard = true;
                    } else if (m_cache_table[start_maddr]->dir_rep ||
                               (m_cache_table[start_maddr]->status != _CACHE_SEND_TREQ &&
                                m_cache_table[start_maddr]->status != _CACHE_WAIT_TREP_OR_RREP))
                    {
                        /* wait for the current entry to proceed */
                        retry = true;
                    }
                }

                if (discard) {

                    if (source == FROM_REMOTE_DIR) {
                        m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                    } else if (source == FROM_REMOTE_DIR_RREP) {
                        m_core_receive_queues[MSG_DIR_RREP]->pop();
                    } else {
                        dir_msg->sent = true;
                    }
                    m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                    continue;
                } else if (retry) {
                    m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                    continue;
                }

                /* accepted as a reply */
                m_cache_table[start_maddr]->dir_rep = dir_msg;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] a ";
                if (dir_msg->type == TREP) {
                    mh_log(4) << "tRep ";
                } else {
                    mh_log(4) << "rRep ";
                }
                mh_log(4) << "arrived from " << dir_msg->sender << " on " << dir_msg->maddr << endl;

                if (source == FROM_REMOTE_DIR) {
                    m_core_receive_queues[MSG_DIR_REQ_REP]->pop();
                } else if (source == FROM_REMOTE_DIR_RREP) {
                    m_core_receive_queues[MSG_DIR_RREP]->pop();
                } else {
                    dir_msg->sent = true;
                }

                m_new_cache_table_entry_schedule_q.erase(m_new_cache_table_entry_schedule_q.begin());
                continue;
            }
        }
    }

    /**************************************/
    /* schedule for directory table space */
    /**************************************/

    /* 1. cache replies is prior to other requests */
    random_shuffle(m_new_dir_table_entry_for_cache_rep_schedule_q.begin(), m_new_dir_table_entry_for_cache_rep_schedule_q.end(), rr_fn);
    while (m_new_dir_table_entry_for_cache_rep_schedule_q.size()) {
        bool is_remote = get<0>(m_new_dir_table_entry_for_cache_rep_schedule_q.front());
        std::shared_ptr<coherenceMsg> cache_rep = get<1>(m_new_dir_table_entry_for_cache_rep_schedule_q.front());
        mh_assert(get_start_maddr_in_line(cache_rep->maddr) == cache_rep->maddr);
        if (m_dir_table.count(cache_rep->maddr)) {
            /* an entry exists in the directorey table */
            if (m_dir_table[cache_rep->maddr]->status == _DIR_WAIT_CACHE_REP &&
                !m_dir_table[cache_rep->maddr]->cache_rep) 
            {
                /* accept when the entry is waiting for one */
                m_dir_table[cache_rep->maddr]->cache_rep = cache_rep;
                if (is_remote) {
                    m_core_receive_queues[MSG_CACHE_REP]->pop();
                } else {
                    cache_rep->sent = true;
                }
            }
            m_new_dir_table_entry_for_cache_rep_schedule_q.erase(m_new_dir_table_entry_for_cache_rep_schedule_q.begin());
            continue;
        } else if (m_dir_table_vacancy_cache_rep_exclusive == 0 && m_dir_table_vacancy_shared == 0)  {
            m_new_dir_table_entry_for_cache_rep_schedule_q.erase(m_new_dir_table_entry_for_cache_rep_schedule_q.begin());
            continue;
        } else {
            bool use_exclusive = m_dir_table_vacancy_cache_rep_exclusive > 0;
            mh_assert(cache_rep->type == INV_REP);
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received an invRep from "
                      << cache_rep->sender << " for address " << cache_rep->maddr << " (on a new entry) " << endl;

            /* create a new entry */
            std::shared_ptr<cacheRequest> l2_req;
            l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(cache_rep->maddr, CACHE_REQ_UPDATE,
                                                               cache_rep->word_count, cache_rep->data));
            l2_req->set_serialization_begin_time(UINT64_MAX);
            l2_req->set_unset_dirty_on_write(false);
            l2_req->set_claim(false);
            l2_req->set_evict(false);
            std::shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence(m_cfg));
            aux_info->core_id = cache_rep->sender;
            aux_info->req_type = UPDATE_FOR_CACHE_REP_ON_NEW;
            l2_req->set_aux_info_for_coherence(aux_info);

            std::shared_ptr<dirTableEntry> new_entry(new dirTableEntry);
            new_entry->cache_req = std::shared_ptr<coherenceMsg>();
            new_entry->bypassed_tReq = std::shared_ptr<coherenceMsg>();
            new_entry->l2_req = l2_req;
            new_entry->cache_req = std::shared_ptr<coherenceMsg>();
            new_entry->bypassed_tReq = std::shared_ptr<coherenceMsg>();
            new_entry->cache_rep = cache_rep;
            new_entry->dir_req = std::shared_ptr<coherenceMsg>();
            new_entry->dir_rep = std::shared_ptr<coherenceMsg>();
            new_entry->dramctrl_req = std::shared_ptr<dramctrlMsg>();
            new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
            new_entry->empty_req = std::shared_ptr<coherenceMsg>();
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
            new_entry->status = _DIR_L2_UPDATE;
            new_entry->substatus = _DIR_L2_UPDATE__CACHE_REP;
            m_dir_table[cache_rep->maddr] = new_entry;

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l2_write_req_schedule_q.push_back(new_entry);
            }

            if (is_remote) {
                m_core_receive_queues[MSG_CACHE_REP]->pop();
            } else {
                cache_rep->sent = true;
            }
            m_new_dir_table_entry_for_cache_rep_schedule_q.erase(m_new_dir_table_entry_for_cache_rep_schedule_q.begin());
            continue;
        }
    }

    /* 2. cache requests/empty requests scheuling */
    random_shuffle(m_new_dir_table_entry_for_req_schedule_q.begin(), m_new_dir_table_entry_for_req_schedule_q.end(), rr_fn);
    while (m_new_dir_table_entry_for_req_schedule_q.size()) {
        dirTableEntrySrc_t source = get<0>(m_new_dir_table_entry_for_req_schedule_q.front());
        std::shared_ptr<coherenceMsg> req = static_pointer_cast<coherenceMsg>(get<1>(m_new_dir_table_entry_for_req_schedule_q.front()));
        mh_assert(req->type != RREQ);
        mh_assert(get_start_maddr_in_line(req->maddr) == req->maddr);
        if (m_dir_table.count(req->maddr)) {
            /* can bypass tReq while a pReq is being blocked */
            bool bypassable_status = (m_dir_table[req->maddr]->status == _DIR_WAIT_TIMESTAMP &&
                                        m_dir_table[req->maddr]->substatus == _DIR_WAIT_TIMESTAMP__CACHE_REQ);
            if (req->type == TREQ && !(m_dir_table[req->maddr]->bypassed_tReq) && bypassable_status) {
                m_dir_table[req->maddr]->bypassed_tReq = req;
                m_dir_table[req->maddr]->bypass_begin_time = system_time;
                mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a bypassable tReq on " 
                          << req->maddr << " for core " << req->sender << " from " << req->sender << endl;
                if (source == FROM_LOCAL_CACHE) {
                    req->sent = true;
                } else if (source == FROM_REMOTE_CACHE_REQ) {
                    m_core_receive_queues[MSG_CACHE_REQ]->pop();
                } else if (source == FROM_REMOTE_CACHE_PREQ) {
                    m_core_receive_queues[MSG_CACHE_PREQ]->pop();
                } else {
                    mh_assert(false);
                }

                m_new_dir_table_entry_for_req_schedule_q.erase(m_new_dir_table_entry_for_req_schedule_q.begin());
                if (stats_enabled() && req->per_mem_instr_stats) {
                    req->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->
                        add_req_nas(system_time - req->birthtime, PTI_STAT_TREQ, 
                                    (req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    if (req->per_mem_instr_stats->is_read()) {
                        stats()->new_read_instr_at_l2((source == FROM_LOCAL_CACHE)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    } else {
                        stats()->new_write_instr_at_l2((source == FROM_LOCAL_CACHE)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }
                }
                continue;
            }
            /* need to finish the previous entry first */
            m_new_dir_table_entry_for_req_schedule_q.erase(m_new_dir_table_entry_for_req_schedule_q.begin());
            continue;
        }
        /* there is no existing entry */
        bool use_exclusive = (req->type == EMPTY_REQ && m_dir_table_vacancy_empty_req_exclusive > 0);
        if (m_dir_table_vacancy_shared == 0 && !use_exclusive) {
            /* no space */
            m_new_dir_table_entry_for_req_schedule_q.erase(m_new_dir_table_entry_for_req_schedule_q.begin());
            continue;
        }
        /* got a space */
        if (source == FROM_LOCAL_CACHE) {
            req->sent = true;
        } else if (source == FROM_REMOTE_CACHE_REQ) {
            m_core_receive_queues[MSG_CACHE_REQ]->pop();
        } else if (source == FROM_REMOTE_CACHE_PREQ) {
            m_core_receive_queues[MSG_CACHE_PREQ]->pop();
        } else {
            mh_assert(false);
        }
        mh_log(5) << "[MEM " << m_id << " @ " << system_time << " ] received a ";
        if (req->type == TREQ) {
            mh_log(5) << "tReq ";
        } else if (req->type == PREQ) {
            mh_log(5) << "pReq ";
        } else if (req->type == EMPTY_REQ) {
            mh_log(5) << "emptyReq ";
        } else {
            mh_assert(false);
        }
        mh_log(5) << "from " << req->sender << " for address " << req->maddr << " (new entry) " << endl;

        /* create a new entry */
        std::shared_ptr<cacheRequest> l2_req;
        std::shared_ptr<dirTableEntry> new_entry(new dirTableEntry);
        if (req->type == EMPTY_REQ) {
            l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(req->maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line, req->data, req->replacing_info));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(true);
            l2_req->set_claim(false);
            l2_req->set_evict(false);
            std::shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence(m_cfg));
            aux_info->replacing_maddr = req->replacing_maddr;
            aux_info->core_id = 0; /* don't care */
            aux_info->req_type = UPDATE_FOR_EMPTY_REQ;
            aux_info->is_replaced_line_dirty = false;
            l2_req->set_aux_info_for_coherence(aux_info);
            new_entry->cache_req = std::shared_ptr<coherenceMsg>();
            new_entry->bypassed_tReq = std::shared_ptr<coherenceMsg>();
            new_entry->cache_rep = std::shared_ptr<coherenceMsg>();
            new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
            new_entry->empty_req = req;
            new_entry->status = _DIR_L2_FOR_EMPTY_REQ;
            new_entry->per_mem_instr_stats = req->per_mem_instr_stats;
            new_entry->block_or_inv_begin_time = system_time;
            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l2_write_req_schedule_q.push_back(new_entry);
            }
            if (stats_enabled() && req->per_mem_instr_stats) {
                req->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->
                    add_req_nas(system_time - req->birthtime, PTI_STAT_EMPTY_REQ, PTI_STAT_LOCAL);
            }
        } else {
            l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(req->maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(false); /* don't care */
            l2_req->set_claim(false);
            l2_req->set_evict(false);
            std::shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence(m_cfg));
            aux_info->core_id = req->sender;
            if (req->type == TREQ) {
                aux_info->req_type = READ_FOR_TREQ;
            } else if (req->type == PREQ) {
                aux_info->req_type = READ_FOR_PREQ;
            } else {
                mh_assert(false);
            }
            l2_req->set_aux_info_for_coherence(aux_info);
            new_entry->cache_req = req;
            new_entry->empty_req = std::shared_ptr<coherenceMsg>();
            new_entry->status = _DIR_L2_FOR_PREQ_OR_TREQ;
            new_entry->per_mem_instr_stats = req->per_mem_instr_stats;
            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(new_entry);
            } else {
                m_l2_write_req_schedule_q.push_back(new_entry);
            }
            if (stats_enabled()) {
                if (req->per_mem_instr_stats) {
                    if (req->type == TREQ) {
                        req->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->
                            add_req_nas(system_time - req->birthtime, PTI_STAT_TREQ, 
                                        (source == FROM_LOCAL_CACHE)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    } else if (req->type == PREQ) {
                        req->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->
                            add_req_nas(system_time - req->birthtime, PTI_STAT_PREQ, 
                                        (source == FROM_LOCAL_CACHE)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    } else {
                        mh_assert(false);
                    }
                    if (req->per_mem_instr_stats->is_read()) {
                        stats()->new_read_instr_at_l2((source == FROM_LOCAL_CACHE)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    } else {
                        stats()->new_write_instr_at_l2((source == FROM_LOCAL_CACHE)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }

                }
            }
        }
        new_entry->l2_req = l2_req;
        new_entry->bypassed_tReq = std::shared_ptr<coherenceMsg>();
        new_entry->cache_rep = std::shared_ptr<coherenceMsg>();
        new_entry->dir_rep = std::shared_ptr<coherenceMsg>();
        new_entry->dramctrl_req = std::shared_ptr<dramctrlMsg>();
        new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
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

    while (m_new_dir_table_entry_for_renewal_schedule_q.size()) {
        dirTableEntrySrc_t source = get<0>(m_new_dir_table_entry_for_renewal_schedule_q.front());
        std::shared_ptr<coherenceMsg> req = 
            static_pointer_cast<coherenceMsg>(get<1>(m_new_dir_table_entry_for_renewal_schedule_q.front()));
        mh_assert(req->type == RREQ);
        mh_assert(get_start_maddr_in_line(req->maddr) == req->maddr);
        bool discard = false;
        bool wait = false;

        if (!m_cfg.allow_revive && *(req->timestamp) <= system_time) {
            /* the line is already expired */
            discard = true;
        } else if (m_dir_table.count(req->maddr)) {
            if (m_dir_table[req->maddr]->cache_req && m_dir_table[req->maddr]->cache_req->type == PREQ) {
                /* discard rReq when a write req is waiting */
                discard = true;
            } else {
                wait = true;
            }
        } else if (m_dir_renewal_table_vacancy == 0) {
            wait = true;
        }

        if (discard) {
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] discarded an rReq on " 
                << req->maddr << " for core " << req->sender << " from " << req->sender << endl;
            if (source == FROM_LOCAL_CACHE) {
                req->sent = true;
            } else if (source == FROM_REMOTE_CACHE_REQ) {
                m_core_receive_queues[MSG_CACHE_REQ]->pop();
            } else if (source == FROM_REMOTE_CACHE_RREQ) {
                m_core_receive_queues[MSG_CACHE_RREQ]->pop();
            } else {
                mh_assert(false);
            }
            m_new_dir_table_entry_for_renewal_schedule_q.erase(m_new_dir_table_entry_for_renewal_schedule_q.begin());
            continue;
        } else if (wait) {
            m_new_dir_table_entry_for_renewal_schedule_q.erase(m_new_dir_table_entry_for_renewal_schedule_q.begin());
            continue;
        }

        /* create an entry to serve an rReq */
        if (source == FROM_LOCAL_CACHE) {
            req->sent = true;
        } else if (source == FROM_REMOTE_CACHE_REQ) {
            m_core_receive_queues[MSG_CACHE_REQ]->pop();
        } else if (source == FROM_REMOTE_CACHE_RREQ) {
            m_core_receive_queues[MSG_CACHE_RREQ]->pop();
        } else {
            mh_assert(false);
        }
        mh_log(5) << "[MEM " << m_id << " @ " << system_time << " ] received an rReq from "
                  << req->sender << " for address " << req->maddr << " (new entry) " << endl;

        /* create a new entry */
        std::shared_ptr<cacheRequest> l2_req;
        l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(req->maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
        l2_req->set_serialization_begin_time(system_time);
        l2_req->set_unset_dirty_on_write(false); /* don't care */
        l2_req->set_claim(false);
        l2_req->set_evict(false);
        std::shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence(m_cfg));
        aux_info->core_id = req->sender;
        aux_info->req_type = READ_FOR_RREQ;
        aux_info->requester_timestamp = *(req->timestamp);
        aux_info->need_to_send_block = false;
        l2_req->set_aux_info_for_coherence(aux_info);

        std::shared_ptr<dirTableEntry> new_entry(new dirTableEntry);
        new_entry->cache_req = req;
        new_entry->empty_req = std::shared_ptr<coherenceMsg>();
        new_entry->status = _DIR_L2_FOR_RREQ;
        new_entry->per_mem_instr_stats = req->per_mem_instr_stats;
        mh_assert(!req->per_mem_instr_stats);
        new_entry->l2_req = l2_req;
        new_entry->bypassed_tReq = std::shared_ptr<coherenceMsg>();
        new_entry->cache_rep = std::shared_ptr<coherenceMsg>();
        new_entry->dir_rep = std::shared_ptr<coherenceMsg>();
        new_entry->dramctrl_req = std::shared_ptr<dramctrlMsg>();
        new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
        new_entry->is_written_back = false;
        new_entry->need_to_writeback_dir = false;

        --m_dir_renewal_table_vacancy;
        new_entry->using_cache_rep_exclusive_space = false;
        new_entry->using_empty_req_exclusive_space = false;

        if (l2_req->use_read_ports()) {
            m_l2_read_req_schedule_q_for_renewal.push_back(new_entry);
        } else {
            m_l2_write_req_schedule_q_for_renewal.push_back(new_entry);
        }

        m_dir_table[req->maddr] = new_entry;

        m_new_dir_table_entry_for_renewal_schedule_q.erase(m_new_dir_table_entry_for_renewal_schedule_q.begin());
        continue;
    }


    /************************************/
    /* scheduling for dramctrl requests */
    /************************************/
    random_shuffle(m_dramctrl_req_schedule_q.begin(), m_dramctrl_req_schedule_q.end(), rr_fn);
    while (m_dramctrl_req_schedule_q.size()) {
        bool is_remote = get<0>(m_dramctrl_req_schedule_q.front());
        std::shared_ptr<dirTableEntry> entry = std::shared_ptr<dirTableEntry>();
        std::shared_ptr<dramctrlMsg> dramctrl_msg = std::shared_ptr<dramctrlMsg>();
        if (is_remote) {
            dramctrl_msg = static_pointer_cast<dramctrlMsg>(get<1>(m_dramctrl_req_schedule_q.front()));
        } else {
            /* check if a writeback exists on the same line - writeback has a higher priority */
            entry = static_pointer_cast<dirTableEntry>(get<1>(m_dramctrl_req_schedule_q.front()));
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
            /* this core has a DRAM controller */
            if (m_dramctrl->available()) {
                if (m_dramctrl_work_table.count(dramctrl_msg->maddr)) {
                    /* processing another request. wait */
                    m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());
                    continue;
                }
                if (dramctrl_msg->dram_req->is_read()) {
                    /* create a new entry */
                        std::shared_ptr<dramctrlTableEntry> new_entry(new dramctrlTableEntry);
                    new_entry->dramctrl_req = dramctrl_msg;
                    new_entry->dramctrl_rep = std::shared_ptr<dramctrlMsg>();
                    new_entry->per_mem_instr_stats = dramctrl_msg->per_mem_instr_stats;
                    new_entry->operation_begin_time= system_time;
                    m_dramctrl_work_table[dramctrl_msg->maddr] = new_entry;
                    if (is_remote && stats_enabled() && new_entry->per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> dir_stat = 
                            new_entry->per_mem_instr_stats->get_tentative_data(T_IDX_DIR);
                        dir_stat->add_req_nas(system_time - dramctrl_msg->birthtime, 
                                              PTI_STAT_DRAMCTRL_READ_REQ, 
                                              is_remote ? PTI_STAT_REMOTE : PTI_STAT_LOCAL);
                    }
                    mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                              << " ] A DRAM read request on " << dramctrl_msg->maddr << " got into the table " << endl;
                } else {
                    /* if write, nothing else to do */
                    mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time 
                              << " ] A DRAM write request on " << dramctrl_msg->maddr << " left for DRAM " << endl;
                }

                m_dramctrl->request(dramctrl_msg->dram_req);

                dramctrl_msg->sent = true;
                if (is_remote) {
                    m_core_receive_queues[MSG_DRAMCTRL_REQ]->pop();
                }
                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());

                if (stats_enabled()) {
                    stats()->add_dram_action();
                }
            } else {
                break;
            }
        } else {
            /* need to send a request to a remote dram controller */
            if (m_core_send_queues[MSG_DRAMCTRL_REQ]->available()) {
                    std::shared_ptr<message_t> msg(new message_t);
                msg->src = m_id;
                msg->dst = m_dramctrl_location;
                msg->type = MSG_DRAMCTRL_REQ;
                uint32_t bytes = 1 + m_cfg.address_size_in_bytes;
                if (!dramctrl_msg->dram_req->is_read()) {
                    bytes += m_cfg.words_per_cache_line * 4;
                }
                msg->flit_count = get_flit_count(bytes);
                msg->content = dramctrl_msg;

                m_core_send_queues[MSG_DRAMCTRL_REQ]->push_back(msg);

                dramctrl_msg->sent = true;

                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] DRAMCTRL ";
                if (dramctrl_msg->dram_req->is_read()) {
                    mh_log(4) << "read ";
                } else {
                    mh_log(4) << "write ";
                } 
                mh_log(4) << "sent " << m_id << " -> " << msg->dst << " num flits " << msg->flit_count << endl;
                m_dramctrl_req_schedule_q.erase(m_dramctrl_req_schedule_q.begin());
            } else {
                /* cannot send now. wait */
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
            std::shared_ptr<cacheTableEntry> entry = m_cat_req_schedule_q.front();
            std::shared_ptr<catRequest> cat_req = entry->cat_req;

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
            std::shared_ptr<cacheTableEntry>& entry = m_l1_read_req_schedule_q.front();
            std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->
                    add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        m_l1->request(l1_req);
        m_l1_read_req_schedule_q.erase(m_l1_read_req_schedule_q.begin());
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
    }
    m_l1_read_req_schedule_q.clear();

    random_shuffle(m_l1_write_req_schedule_q.begin(), m_l1_write_req_schedule_q.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q.size()) {
            std::shared_ptr<cacheTableEntry>& entry = m_l1_write_req_schedule_q.front();
            std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->
                    add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        m_l1->request(l1_req);
        m_l1_write_req_schedule_q.erase(m_l1_write_req_schedule_q.begin());
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
    }
    m_l1_write_req_schedule_q.clear();

    /******************************/
    /* scheduling for L1, renewal */
    /******************************/
    random_shuffle(m_l1_read_req_schedule_q_for_renewal.begin(), m_l1_read_req_schedule_q_for_renewal.end(), rr_fn);
    while (m_l1->read_port_available() && m_l1_read_req_schedule_q_for_renewal.size()) {
            std::shared_ptr<cacheTableEntry>& entry = m_l1_read_req_schedule_q_for_renewal.front();
            std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->
                    add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        m_l1->request(l1_req);
        m_l1_read_req_schedule_q_for_renewal.erase(m_l1_read_req_schedule_q_for_renewal.begin());
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
    }
    m_l1_read_req_schedule_q_for_renewal.clear();

    random_shuffle(m_l1_write_req_schedule_q_for_renewal.begin(), m_l1_write_req_schedule_q_for_renewal.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q_for_renewal.size()) {
            std::shared_ptr<cacheTableEntry>& entry = m_l1_write_req_schedule_q_for_renewal.front();
            std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;

        if (l1_req->serialization_begin_time() == UINT64_MAX) {
            l1_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l1_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_L1)->
                    add_l1_srz(system_time - l1_req->serialization_begin_time());
            }
        }
        m_l1->request(l1_req);
        m_l1_write_req_schedule_q_for_renewal.erase(m_l1_write_req_schedule_q_for_renewal.begin());
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
    }
    m_l1_write_req_schedule_q_for_renewal.clear();

    /*********************/
    /* scheduling for L2 */
    /*********************/
    random_shuffle(m_l2_read_req_schedule_q.begin(), m_l2_read_req_schedule_q.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q.size()) {
            std::shared_ptr<dirTableEntry>& entry = m_l2_read_req_schedule_q.front();
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
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->get_tentative_data(T_IDX_L2)->
                    add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        m_l2->request(l2_req);
        m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
    }
    m_l2_read_req_schedule_q.clear();

    random_shuffle(m_l2_write_req_schedule_q.begin(), m_l2_write_req_schedule_q.end(), rr_fn);
    while (m_l2->write_port_available() && m_l2_write_req_schedule_q.size()) {
            std::shared_ptr<dirTableEntry>& entry = m_l2_write_req_schedule_q.front();
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
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->get_tentative_data(T_IDX_L2)->
                    get_tentative_data(T_IDX_L2)->add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        m_l2->request(l2_req);
        m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
    }
    m_l2_write_req_schedule_q.clear();

    /******************************/
    /* scheduling for L2, renewal */
    /******************************/
    random_shuffle(m_l2_read_req_schedule_q_for_renewal.begin(), m_l2_read_req_schedule_q_for_renewal.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q_for_renewal.size()) {
            std::shared_ptr<dirTableEntry>& entry = m_l2_read_req_schedule_q_for_renewal.front();
            std::shared_ptr<cacheRequest>& l2_req = entry->l2_req;
        if (m_l2_writeback_status.count(get_start_maddr_in_line(l2_req->maddr())) > 0) {
            m_l2_read_req_schedule_q_for_renewal.erase(m_l2_read_req_schedule_q_for_renewal.begin());
            continue;
        }
        if (l2_req->serialization_begin_time() == UINT64_MAX) {
            l2_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l2_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->get_tentative_data(T_IDX_L2)->
                    add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        m_l2->request(l2_req);
        m_l2_read_req_schedule_q_for_renewal.erase(m_l2_read_req_schedule_q_for_renewal.begin());
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
    }
    m_l2_read_req_schedule_q_for_renewal.clear();

    random_shuffle(m_l2_write_req_schedule_q_for_renewal.begin(), m_l2_write_req_schedule_q_for_renewal.end(), rr_fn);
    while (m_l2->write_port_available() && m_l2_write_req_schedule_q_for_renewal.size()) {
            std::shared_ptr<dirTableEntry>& entry = m_l2_write_req_schedule_q_for_renewal.front();
            std::shared_ptr<cacheRequest>& l2_req = entry->l2_req;

        if (m_l2_writeback_status.count(get_start_maddr_in_line(l2_req->maddr())) > 0 &&
            m_l2_writeback_status[get_start_maddr_in_line(l2_req->maddr())] != entry)
        {
            m_l2_write_req_schedule_q_for_renewal.erase(m_l2_write_req_schedule_q_for_renewal.begin());
            continue;
        }

        if (l2_req->serialization_begin_time() == UINT64_MAX) {
            l2_req->set_operation_begin_time(UINT64_MAX);
        } else {
            l2_req->set_operation_begin_time(system_time);
            if (stats_enabled() && entry->per_mem_instr_stats) {
                entry->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->get_tentative_data(T_IDX_L2)->
                    get_tentative_data(T_IDX_L2)->add_l2_srz(system_time - l2_req->serialization_begin_time());
            }
        }
        m_l2->request(l2_req);
        m_l2_write_req_schedule_q_for_renewal.erase(m_l2_write_req_schedule_q_for_renewal.begin());
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
    }
    m_l2_write_req_schedule_q_for_renewal.clear();

    /*******************************************/
    /* scheduling for sending dramctrl replies */
    /*******************************************/
    random_shuffle(m_dramctrl_rep_schedule_q.begin(), m_dramctrl_rep_schedule_q.end(), rr_fn);
    while (m_dramctrl_rep_schedule_q.size()) {
            std::shared_ptr<dramctrlTableEntry>& entry = m_dramctrl_rep_schedule_q.front();
            std::shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
            std::shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;
        if (dramctrl_req->sender == m_id) {
            mh_assert(m_dir_table.count(dramctrl_req->maddr));
            m_dir_table[dramctrl_req->maddr]->dramctrl_rep = entry->dramctrl_rep;
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

    /* END of schedule_request() */
}

void privateSharedPTI::update_cache_table() {

    for (cacheTable::iterator it_addr = m_cache_table.begin(); it_addr != m_cache_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        std::shared_ptr<cacheTableEntry>& entry = it_addr->second;

        std::shared_ptr<memoryRequest>& core_req = entry->core_req;
        std::shared_ptr<catRequest>& cat_req = entry->cat_req;

        std::shared_ptr<cacheRequest>& l1_req = entry->l1_req;
        std::shared_ptr<coherenceMsg>& dir_req = entry->dir_req;
        std::shared_ptr<coherenceMsg>& dir_rep = entry->dir_rep;
        std::shared_ptr<coherenceMsg>& cache_req = entry->cache_req;
        std::shared_ptr<coherenceMsg>& cache_rep = entry->cache_rep;

        std::shared_ptr<cacheLine> l1_line = (l1_req)? l1_req->line_copy() : std::shared_ptr<cacheLine>();
        std::shared_ptr<cacheLine> l1_victim = (l1_req)? l1_req->line_to_evict_copy() : std::shared_ptr<cacheLine>();
        std::shared_ptr<cacheCoherenceInfo> l1_line_info = 
            (l1_line)? static_pointer_cast<cacheCoherenceInfo>(l1_line->coherence_info) : std::shared_ptr<cacheCoherenceInfo>();
        std::shared_ptr<cacheCoherenceInfo> l1_victim_info = 
            (l1_victim)? static_pointer_cast<cacheCoherenceInfo>(l1_victim->coherence_info) : std::shared_ptr<cacheCoherenceInfo>();

        std::shared_ptr<privateSharedPTIStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        bool dir_rep_ready_for_local_read = false;
        bool check_to_send_rReq = false;

        //cerr << "@ " << system_time << " entry on " << start_maddr << " status " << entry->status  << endl;

        if (entry->status == _CACHE_CAT_AND_L1_FOR_READ) {

            uint32_t home;

            if (l1_req->status() == CACHE_REQ_HIT) {

                /* aux stat */
                l1_line_info->last_access = system_time;

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 read HIT on " << core_req->maddr() << endl;
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> l1_stat = per_mem_instr_stats->get_tentative_data(T_IDX_L1);
                        l1_stat->add_l1_ops(system_time - l1_req->operation_begin_time());
                        if (l1_line_info->status == TIMESTAMPED) {
                            l1_stat->add_l1_for_read_hit_on_T(l1_stat->total_cost());
                        } else {
                            l1_stat->add_l1_for_read_hit_on_P(l1_stat->total_cost());
                        }
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                    }
                    if (l1_line_info->status == TIMESTAMPED) {
                        stats()->hit_on_T_for_read_instr_at_l1();
                    } else {
                        stats()->hit_on_P_for_read_instr_at_l1();
                    }
                    stats()->did_finish_read();
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }

                home = l1_line_info->home;
                cat_req = std::shared_ptr<catRequest>();

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l1_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);

                if (m_cfg.renewal_type == _RENEWAL_SYNCHED) {

                    check_to_send_rReq = true;
                    /* will continue to the last of the loop and will decide to send a rReq */

                } else {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finishes serving a read on "
                        << core_req->maddr() << endl;
                    if (stats_enabled() && per_mem_instr_stats) {
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                    set_req_status(core_req, REQ_DONE);

                    ++m_available_core_ports;
                    ++m_cache_table_vacancy;
                    m_cache_table.erase(it_addr++);
                    continue;
                    /* FINISHED */
                }

            } else if (l1_req->status() == CACHE_REQ_MISS && l1_line) {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a local L1 read MISS for expired timestamp on "
                          << core_req->maddr() << endl;
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> l1_stat = per_mem_instr_stats->get_tentative_data(T_IDX_L1);
                        l1_stat->add_l1_ops(system_time - l1_req->operation_begin_time());
                        l1_stat->add_l1_for_read_hit_on_T(l1_stat->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                    }
                    stats()->miss_on_expired_T_for_read_instr_at_l1();
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }
#ifdef ADDITIONAL_INSTRUMENT
                cout << "RC " << start_maddr << " ";
                if (m_renewal_count.count(start_maddr) == 0) {
                    cout << "0";
                } else {
                    cout << m_renewal_count[start_maddr];
                    m_renewal_count.erase(start_maddr);
                }
                cout << " for expiration " << endl;
#endif 

                mh_assert(l1_line_info->status == TIMESTAMPED);
                home = l1_line_info->home;
                cat_req = std::shared_ptr<catRequest>();

                cache_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_req->sender = m_id;
                cache_req->receiver = home;
                cache_req->type = TREQ;
                cache_req->maddr = start_maddr;
                cache_req->sent = false;
                cache_req->per_mem_instr_stats = per_mem_instr_stats;
                cache_req->word_count = 0;
                cache_req->data = shared_array<uint32_t>();
                cache_req->birthtime = system_time;
                m_cache_req_schedule_q.push_back(entry);

                entry->short_latency_begin_time = system_time;
                entry->status = _CACHE_SEND_TREQ;
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
                    /* will not continue without CAT info */
                    home = cat_req->home();

                    if (l1_req->status() == CACHE_REQ_MISS) {

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a true read miss on address "
                                  << core_req->maddr() << endl;
                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> l1_stat = 
                                    per_mem_instr_stats->get_tentative_data(T_IDX_L1);
                                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> c_stat = 
                                    per_mem_instr_stats->get_tentative_data(T_IDX_CAT);
                                if (per_mem_instr_stats->get_max_tentative_data_index() == T_IDX_L1) {
                                    per_mem_instr_stats->add_l1_for_read_miss_true(l1_stat->total_cost());
                                }
                                per_mem_instr_stats->commit_max_tentative_data();
                            }
                            stats()->miss_true_for_read_instr_at_l1();
                        } else if (per_mem_instr_stats) {
                            per_mem_instr_stats->clear_tentative_data();
                        }

                        /* could migrate out here */

                        if (dir_rep && dir_rep->data) {
                            entry->short_latency_begin_time = system_time;
                            dir_rep_ready_for_local_read = true;
                            /* will continue to the last of the loop and process the dir rep */
                        } else {
                            cache_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                            cache_req->sender = m_id;
                            cache_req->receiver = home;
                            cache_req->type = TREQ;
                            cache_req->maddr = start_maddr;
                            cache_req->sent = false;
                            cache_req->per_mem_instr_stats = per_mem_instr_stats;
                            cache_req->word_count = 0;
                            cache_req->data = shared_array<uint32_t>();
                            cache_req->birthtime = system_time;
                            cache_req->requested_time = system_time;
                            m_cache_req_schedule_q.push_back(entry);

                            entry->short_latency_begin_time = system_time;
                            entry->status = _CACHE_SEND_TREQ;
                            ++it_addr;
                            continue;
                            /* TRANSITION */
                        }
                    } else {
                        if (l1_req->status() == CACHE_REQ_NEW) {
                            /* the L1 request has lost an arbitration - retry */
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
                } else {
                    /* CAT is not finished */
                    if (cat_req->status() == CAT_REQ_NEW) {
                        /* the cat request has lost an arbitration - retry */
                        m_cat_req_schedule_q.push_back(entry);
                    }
                    if (l1_req->status() == CACHE_REQ_NEW) {
                        /* the L1 request has lost an arbitration - retry */
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
            }

            /* _CACHE_CAT_AND_L1_FOR_READ */

        } else if (entry->status == _CACHE_SEND_TREQ) {

            if (dir_rep) {
                entry->short_latency_begin_time = system_time;
                dir_rep_ready_for_local_read = true;
                /* will continue to the last of the loop and process the dir rep */
            } else {
                if (cache_req->sent) {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent a tReq to " 
                              << cache_req->receiver << " for " << cache_req->maddr << endl;
                    if (stats_enabled()) {
                        stats()->req_sent(PTI_STAT_TREQ, (cache_req->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }
                    entry->status = _CACHE_WAIT_TREP_OR_RREP;
                    ++it_addr;
                    continue;
                    /* TRANSITION */
                } else {
                    m_cache_req_schedule_q.push_back(entry);
                    ++it_addr;
                    continue;
                    /* SPIN */
                }
            }

            /* CACHE_SEND_TREQ */

        } else if (entry->status == _CACHE_WAIT_TREP_OR_RREP) {

            if (!dir_rep) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (dir_rep->type == PREP) {
                /* the directory may decide to give a pRep for a tReq. */
                /* restart for pRep in this case */
                entry->status = _CACHE_WAIT_PREP;
                continue;
                /* RETRY THIS ENTRY */
            } else {
                dir_rep_ready_for_local_read = true;
                /* will continue to the last of the loop and process the dir rep */
            }

            /* CACHE_WAIT_TREP_OR_RREP */

        } else if (entry->status == _CACHE_CAT_AND_L1_FOR_WRITE) {

            uint32_t home;

            if (l1_req->status() == CACHE_REQ_HIT) {

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a L1 write HIT on " << core_req->maddr() << endl;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finishes serving a write on " << core_req->maddr() << endl;
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> l1_stat = per_mem_instr_stats->get_tentative_data(T_IDX_L1);
                        l1_stat->add_l1_ops(system_time - l1_req->operation_begin_time());  
                        l1_stat->add_l1_for_write_hit(l1_stat->total_cost());
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                    stats()->hit_for_write_instr_at_l1();
                    stats()->did_finish_write();
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }

                cat_req = std::shared_ptr<catRequest>();

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l1_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);
                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;

                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISHED */

            } else {
                /* record when CAT/L1 is finished */
                if (cat_req->operation_begin_time() != UINT64_MAX && cat_req->status() == CAT_REQ_DONE) {
                    if (stats_enabled() && per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> cat_stats = per_mem_instr_stats->get_tentative_data(T_IDX_CAT);
                        cat_stats->add_cat_ops(system_time - cat_req->operation_begin_time());
                    }
                    cat_req->set_operation_begin_time(UINT64_MAX);
                }
                if (l1_req->operation_begin_time() != UINT64_MAX && l1_req->status() == CACHE_REQ_MISS) {
                    if (stats_enabled() && per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> l1_stats = per_mem_instr_stats->get_tentative_data(T_IDX_L1);
                        l1_stats->add_l1_ops(system_time - l1_req->operation_begin_time());
                    }
                    l1_req->set_operation_begin_time(UINT64_MAX);
                }

                if (cat_req->status() == CAT_REQ_DONE) {
                    /* will not continue without CAT info */
                    home = cat_req->home();

                    if (l1_req->status() == CACHE_REQ_MISS) {

#ifdef ADDITIONAL_INSTRUMENT
                        if (l1_line) {
                            cout << "RC " << start_maddr << " ";
                            if (m_renewal_count.count(start_maddr) == 0) {
                                cout << "0";
                            } else {
                                cout << m_renewal_count[start_maddr];
                                m_renewal_count.erase(start_maddr);
                            }
                            cout << " for switch " << endl;
                        }
#endif 
                        if (l1_line) {
                            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a write miss on T for address "
                                      << core_req->maddr() << endl;
                        } else {
                            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a true write miss for address "
                                      << core_req->maddr() << endl;
                        }

                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> l1_stat = 
                                    per_mem_instr_stats->get_tentative_data(T_IDX_L1);
                                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> c_stat = 
                                    per_mem_instr_stats->get_tentative_data(T_IDX_CAT);
                                if (per_mem_instr_stats->get_max_tentative_data_index() == T_IDX_L1) {
                                    if (l1_line) {
                                        per_mem_instr_stats->add_l1_for_write_miss_on_T(l1_stat->total_cost());
                                    } else {
                                        per_mem_instr_stats->add_l1_for_write_miss_true(l1_stat->total_cost());
                                    }
                                }
                                per_mem_instr_stats->commit_max_tentative_data();
                            }
                            if (l1_line) {
                                stats()->miss_on_T_for_write_instr_at_l1();
                            } else {
                                stats()->miss_true_for_write_instr_at_l1();
                            }
                        } else if (per_mem_instr_stats) {
                            per_mem_instr_stats->clear_tentative_data();
                        }

                        /* could migrate out here */

                        if (l1_line && m_cfg.renewal_type == _RENEWAL_SCHEDULED) {
                            /* will switch to P mode - no need to send rReq any more */
                            m_rReq_schedule_q.remove(start_maddr);
                        }

                        cache_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                        cache_req->sender = m_id;
                        cache_req->receiver = home;
                        cache_req->type = PREQ;
                        cache_req->maddr = start_maddr;
                        cache_req->sent = false;
                        cache_req->per_mem_instr_stats = per_mem_instr_stats;
                        cache_req->word_count = 0;
                        cache_req->data = shared_array<uint32_t>();
                        cache_req->birthtime = system_time;
                        m_cache_req_schedule_q.push_back(entry);

                        entry->short_latency_begin_time = system_time;
                        entry->status = _CACHE_SEND_PREQ;
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

            /* _CACHE_CAT_AND_L1_FOR_WRITE */

        } else if (entry->status == _CACHE_SEND_PREQ) {

            if (cache_req->sent) {
                entry->status = _CACHE_WAIT_PREP;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent a pReq to " 
                          << cache_req->receiver << " for " << cache_req->maddr << endl;
                if (stats_enabled()) {
                    stats()->req_sent(PTI_STAT_PREQ, (cache_req->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                }
                ++it_addr;
                continue;
                /* TRANSITION */
            } else {
                m_cache_req_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            /* CACHE_SEND_PREQ */

        } else if (entry->status == _CACHE_WAIT_PREP) {

            if (!dir_rep) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (stats_enabled() && per_mem_instr_stats) {
                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> dir_stat = per_mem_instr_stats->get_tentative_data(T_IDX_DIR);
                per_mem_instr_stats->commit_tentative_data(T_IDX_DIR);
                per_mem_instr_stats->add_rep_nas(system_time - dir_rep->birthtime, 
                                                 PTI_STAT_PREP, 
                                                 (dir_rep->sender == m_id) ? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
            }

            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a pRep on " << start_maddr << endl;

            shared_array<uint32_t> data = dir_rep->data;
            mh_assert(dir_rep->type == PREP);
            if (!core_req->is_read()) {
                uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    data[i + word_offset] = core_req->data()[i];
                }
            }

            std::shared_ptr<cacheCoherenceInfo> new_info(new cacheCoherenceInfo);
            new_info->home = dir_rep->sender;
            new_info->status = PRIVATE;
            new_info->timestamp = dir_rep->timestamp;
            l1_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line, 
                                                               dir_rep->data, new_info));
            l1_req->set_serialization_begin_time(system_time);
            l1_req->set_unset_dirty_on_write(core_req->is_read());
            l1_req->set_claim(true);
            l1_req->set_evict(true);
            l1_req->set_aux_info_for_coherence(std::shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(UPDATE_FOR_DIR_REP)));
            if (l1_req->use_read_ports()) {
                m_l1_read_req_schedule_q.push_back(entry);
            } else {
                m_l1_write_req_schedule_q.push_back(entry);
            }

            entry->status = _CACHE_UPDATE_L1;
            ++it_addr;
            continue;
            /* TRANSITION */

            /* _CACHE_WAIT_PREP */

        } else if (entry->status == _CACHE_SEND_RREQ) {
            if (cache_req->sent) {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent an rReq to " 
                          << cache_req->receiver << " for " << cache_req->maddr << endl;
                if (stats_enabled()) {
                    stats()->req_sent(PTI_STAT_RREQ, (cache_req->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    if (per_mem_instr_stats) {
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                }

                if (core_req) {
                    mh_assert(core_req->is_read());
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time 
                              << " ] finishes serving a read on " << core_req->maddr() << endl;
                    set_req_status(core_req, REQ_DONE);
                    ++m_cache_renewal_table_vacancy;
                    m_cache_table.erase(it_addr++);
                    continue;
                    /* FINISH */
                } else {
                    /* a scheduled sending */
                    ++m_cache_renewal_table_vacancy;
                    m_cache_table.erase(it_addr++);
                    continue;
                    /* FINISH */
                }

            } else if (m_cfg.retry_rReq) {
                m_cache_req_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            } else {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] failed to send an rReq to " 
                          << cache_req->receiver << " for " << cache_req->maddr << endl;
                ++m_cache_renewal_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISH */
            }

            /* CACHE_SEND_RREQ */

        } else if (entry->status == _CACHE_SEND_CACHE_REP) {

            if (!cache_rep->sent) {
                m_cache_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent a ";
            if (cache_rep->type == INV_REP) {
                mh_log(4) << "invRep ";
            } else {
                mh_assert(cache_rep->type == SWITCH_REP);
                mh_log(4) << "switchRep ";
            } 
            mh_log(4) << "for " << cache_rep->maddr << " to " << cache_rep->receiver << endl;
            if (stats_enabled()) {
                if (cache_rep->type == INV_REP) {
                    stats()->rep_sent(PTI_STAT_INV_REP, (cache_rep->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                } else {
                    stats()->rep_sent(PTI_STAT_SWITCH_REP, (cache_rep->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                }
            }

            if (dir_req) {
                /* serving an invReq or switcReq */
                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISHED */

            } else {
                /* evicted a private line */
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->add_rep_nas(system_time - cache_rep->birthtime,
                                                         PTI_STAT_INV_REP, 
                                                         (cache_rep->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }
                }

                mh_assert(core_req);
                if (l1_line_info->status == TIMESTAMPED && m_cfg.renewal_type == _RENEWAL_SYNCHED) {
                    check_to_send_rReq = true;
                    /* will continue to the last of the loop and will decide to send a rReq */
                } else {
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finishes serving a ";
                    if (core_req->is_read()) {
                        mh_log(4) << "read on ";
                    } else {
                        mh_log(4) << "write on ";
                    }
                    mh_log(4) << core_req->maddr() << endl;

                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                            stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                        }
                    }

                    set_req_status(core_req, REQ_DONE);
                    ++m_available_core_ports;
                    ++m_cache_table_vacancy;
                    m_cache_table.erase(it_addr++);
                    continue;
                    /* FINISHED */
                }
            }

            /* _CACHE_SEND_CACHE_REP */

        } else if (entry->status == _CACHE_L1_FOR_DIR_REQ)  {

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

            mh_assert(!per_mem_instr_stats);

            if (l1_req->status() == CACHE_REQ_HIT) {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] a ";
                if (dir_req->type == INV_REQ) {
                    mh_log(4) << "invReq ";
                } else {
                    mh_log(4) << "switchReq ";
                }
                mh_log(4) << "on " << start_maddr << " gets an L1 hit " << endl;

                uint32_t home = l1_line_info->home;
                cache_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_rep->sender = m_id;
                cache_rep->receiver = home;
                if (dir_req->type == INV_REQ) {
                    cache_rep->type = INV_REP;
                    if (stats_enabled()) {
                        stats()->evict_from_l1((home == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }
                } else {
                    cache_rep->type = SWITCH_REP;
                }

                if (m_cfg.renewal_type == _RENEWAL_SCHEDULED && dir_req->type == SWITCH_REQ) {
                        std::shared_ptr<coherenceMsg> new_rReq(new coherenceMsg);
                    new_rReq->sender = m_id;
                    new_rReq->receiver = home;
                    new_rReq->type = RREQ;
                    new_rReq->maddr = start_maddr;
                    new_rReq->data = shared_array<uint32_t>();
                    new_rReq->sent = false;
                    new_rReq->timestamp = std::shared_ptr<uint64_t>(new uint64_t(*(dir_req->timestamp)));
                    new_rReq->requested_time = max(system_time, *(dir_req->timestamp) - m_cfg.renewal_threshold);

                    m_rReq_schedule_q.set(new_rReq);
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] scheduled a rReq on " << new_rReq->maddr
                              << " at " << new_rReq->requested_time << endl;
                }

                cache_rep->maddr = start_maddr;
                cache_rep->sent = false;
                cache_rep->per_mem_instr_stats = per_mem_instr_stats;
                cache_rep->birthtime = system_time;

                if (!l1_line->data_dirty) {
                    cache_rep->word_count = 0;
                    cache_rep->data = shared_array<uint32_t>();
                } else {
                    if (stats_enabled()) {
                        stats()->writeback_from_l1((home == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }
                    cache_rep->word_count = m_cfg.words_per_cache_line;
                    cache_rep->data = l1_line->data;
                }
                m_cache_rep_schedule_q.push_back(entry);

                entry->status = _CACHE_SEND_CACHE_REP;
                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                /* line was evicted and a cache reply is already sent */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] discarded a ";
                if (dir_req->type == INV_REQ) {
                    mh_log(4) << "invReq ";
                } else {
                    mh_log(4) << "switchReq ";
                }
                mh_log(4) << "on " << start_maddr << " as it was already invalidated." << endl;
                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISH */
            }

        } else if (entry->status == _CACHE_L1_FOR_RENEWAL)  {

            if (l1_req->status() == CACHE_REQ_NEW) {
                /* the L1 request has lost an arbitration - retry */
                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q_for_renewal.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q_for_renewal.push_back(entry);
                }
                ++it_addr;
                continue;
                /* SPIN */
            } else if (l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_assert(!per_mem_instr_stats);

            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] a ";
            if (dir_req->type == TREP) {
                mh_log(4) << "tRep ";
            } else {
                mh_log(4) << "rRep ";
            }
            mh_log(4) << "on " << start_maddr << " ";
            if (l1_req->status() == CACHE_REQ_HIT) {
                mh_assert(l1_line_info->status == TIMESTAMPED);
                mh_log(4) << "gets an L1 hit and renewed timestamp " << *(l1_line_info->timestamp) << endl;
#ifdef ADDITIONAL_INSTRUMENT
                if (m_renewal_count.count(start_maddr) == 0) {
                    m_renewal_count[start_maddr] = 1;
                } else {
                    m_renewal_count[start_maddr] += 1;
                }
#endif 
            } else {
                mh_log(4) << "gets an L1 miss and discarded " << endl;
            }

            if (m_cfg.renewal_type == _RENEWAL_SCHEDULED && l1_req->status() == CACHE_REQ_HIT) {
                    std::shared_ptr<coherenceMsg> new_rReq(new coherenceMsg);
                new_rReq->sender = m_id;
                new_rReq->receiver = dir_req->sender;
                new_rReq->type = RREQ;
                new_rReq->maddr = start_maddr;
                new_rReq->data = shared_array<uint32_t>();
                new_rReq->sent = false;
                new_rReq->timestamp = std::shared_ptr<uint64_t>(new uint64_t(*(l1_line_info->timestamp)));
                new_rReq->requested_time = max(system_time, *(l1_line_info->timestamp) - m_cfg.renewal_threshold);

                m_rReq_schedule_q.set(new_rReq);
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] scheduled a rReq on " << new_rReq->maddr
                          << " at " << new_rReq->requested_time << endl;
            }

            if (core_req && m_cfg.renewal_type == _RENEWAL_SYNCHED) {
                /* this renewal was done right after processing a core request */
                check_to_send_rReq = true;
            } else {
                ++m_cache_renewal_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISH */
            }
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
                if (stats_enabled()) {
                    if (core_req->is_read()) {
                        stats()->did_finish_read();
                    } else {
                        stats()->did_finish_write();
                    }
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->get_tentative_data(T_IDX_L1)->add_l1_ops(system_time - l1_req->operation_begin_time());
                        if (core_req->is_read()) {
                            per_mem_instr_stats->
                                add_l1_for_feed_for_read(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        } else {
                            per_mem_instr_stats->
                                add_l1_for_feed_for_write(per_mem_instr_stats->get_tentative_data(T_IDX_L1)->total_cost());
                        }
                        per_mem_instr_stats->commit_tentative_data(T_IDX_L1);
                    }
                } else if (per_mem_instr_stats) {
                    per_mem_instr_stats->clear_tentative_data();
                }

                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = l1_line->data[i + word_offset];
                }
                set_req_data(core_req, ret);

                if (!l1_victim || l1_victim_info->status == TIMESTAMPED) {
                    if (l1_victim) {
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] evicted a cache line in T on "
                                  << l1_victim->start_maddr << endl;
                        if (stats_enabled()) {
                            stats()->evict_from_l1((l1_victim_info->home == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                        }
#ifdef ADDITIONAL_INSTRUMENT
                        cout << "RC " << l1_victim->start_maddr << " ";
                        if (m_renewal_count.count(l1_victim->start_maddr) == 0) {
                            cout << "0";
                        } else {
                            cout << m_renewal_count[l1_victim->start_maddr];
                            m_renewal_count.erase(l1_victim->start_maddr);
                        }
                        cout << " for eviction " << endl;
#endif 
                    }

                    if (l1_victim && m_cfg.renewal_type == _RENEWAL_SCHEDULED) {
                        m_rReq_schedule_q.remove(l1_victim->start_maddr);
                        /* no need to send rReq any more */
                    }

                    if (l1_line_info->status == TIMESTAMPED && m_cfg.renewal_type == _RENEWAL_SYNCHED) {
                        mh_assert(core_req->is_read());
                        check_to_send_rReq = true;
                        /* will continue to the last of the loop and will decide to send a rReq */
                    } else {

                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] updated for a ";
                        if (dir_rep->type == TREP) {
                            mh_log(4) << "tRep ";
                        } else if (dir_rep->type == PREP) {
                            mh_log(4) << "pRep ";
                        } else if (dir_rep->type == RREP) {
                            mh_log(4) << "rRep ";
                        } 
                        mh_log(4) << "on " << dir_rep->maddr  << endl;
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finishes serving a ";
                        if (core_req->is_read()) {
                            mh_log(4) << "read on ";
                        } else {
                            mh_log(4) << "write on ";
                        }
                        mh_log(4) << core_req->maddr() << endl;

                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                            }
                        }

                        set_req_status(core_req, REQ_DONE);
                        ++m_available_core_ports;
                        ++m_cache_table_vacancy;
                        m_cache_table.erase(it_addr++);
                        continue;
                        /* FINISHED */
                    }
                } else {
                    /* we have a victim in P */
                    uint32_t home = l1_victim_info->home;

                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] is evicting a cache line in P on " 
                        << l1_victim->start_maddr << endl;
                    if (stats_enabled()) {
                        stats()->evict_from_l1((home == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }

                    cache_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                    cache_rep->sender = m_id;
                    cache_rep->receiver = home;
                    cache_rep->type = INV_REP;
                    cache_rep->maddr = l1_victim->start_maddr;
                    cache_rep->sent = false;
                    cache_rep->per_mem_instr_stats = per_mem_instr_stats;
                    cache_rep->birthtime = system_time;

                    if (!l1_victim->data_dirty) {
                        cache_rep->word_count = 0;
                        cache_rep->data = shared_array<uint32_t>();
                    } else {
                        if (stats_enabled()) {
                            stats()->writeback_from_l1((home == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                        }
                        cache_rep->word_count = m_cfg.words_per_cache_line;
                        cache_rep->data = l1_victim->data;
                    }
                    m_cache_rep_schedule_q.push_back(entry);

                    entry->status = _CACHE_SEND_CACHE_REP;
                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }
            }

            /* _CACHE_UPDATE_L1 */

        }

        /* reach here and process a received dir_rep */
        if (dir_rep_ready_for_local_read) {

            if (stats_enabled()) {
                bool shortened = dir_rep->type == RREP ||
                                 !cache_req ||
                                 cache_req->birthtime != dir_rep->requested_time;
                if (shortened) {
                    stats()->short_tReq();
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->add_short_latency(system_time - entry->short_latency_begin_time);
                    }
                } else {
                    if (per_mem_instr_stats) {
                        per_mem_instr_stats->commit_tentative_data(T_IDX_DIR);
                        per_mem_instr_stats->add_rep_nas(system_time - dir_rep->birthtime,
                                                         PTI_STAT_TREP, 
                                                         (dir_rep->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                    }
                }
                stats()->did_finish_read();
            }

            shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
            uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
            for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                if (dir_rep->data) {
                    ret[i] = dir_rep->data[i + word_offset];
                } else {
                    /* in case of a miss on expired T */
                    ret[i] = l1_line->data[i + word_offset];
                }
            }
            set_req_data(core_req, ret);

            if (*(dir_rep->timestamp) <= system_time) {
                /* expired - take a word */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received an UNCACHEABLE";
                if (dir_rep->requested_time != cache_req->birthtime) {
                    mh_log(4) << ", SHORTENED";
                }
                if (dir_rep->type == TREP) {
                    mh_log(4) << " tRep ";
                } else {
                    mh_log(4) << " rRep ";
                }
                mh_log(4) << "on " << core_req->maddr();
                mh_log(4) << " (TIMESTAMP: " << *(dir_rep->timestamp) << " ) "  << endl;

                /* could be expired because there are writes waiting. do not send rReq */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time 
                          << " ] finishes serving a read on " << core_req->maddr() << endl;
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                    }
                }

                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;
                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISH */

            } else {
                /* cacheable */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a CACHEABLE";
                if (dir_rep->requested_time != cache_req->birthtime) {
                    mh_log(4) << ", SHORTENED";
                }
                if (dir_rep->type == TREP) {
                    mh_log(4) << " tRep ";
                } else {
                    mh_log(4) << " rRep ";
                }
                mh_log(4) << "on " << core_req->maddr();
                mh_log(4) << " (TIMESTAMP: " << *(dir_rep->timestamp) << " ) "  << dir_rep->timestamp << endl;

                std::shared_ptr<cacheCoherenceInfo> new_info(new cacheCoherenceInfo);
                new_info->status = TIMESTAMPED;
                new_info->home = dir_rep->sender;
                new_info->timestamp = dir_rep->timestamp;

                if (m_cfg.renewal_type == _RENEWAL_SCHEDULED) {
                        std::shared_ptr<coherenceMsg> new_rReq(new coherenceMsg);
                    new_rReq->sender = m_id;
                    new_rReq->receiver = dir_rep->sender;
                    new_rReq->type = RREQ;
                    new_rReq->maddr = start_maddr;
                    new_rReq->data = shared_array<uint32_t>();
                    new_rReq->sent = false;
                    new_rReq->timestamp = std::shared_ptr<uint64_t>(new uint64_t(*(dir_rep->timestamp)));
                    new_rReq->requested_time = max(system_time, *(dir_rep->timestamp) - m_cfg.renewal_threshold);

                    m_rReq_schedule_q.set(new_rReq);
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] scheduled a rReq on " << new_rReq->maddr
                              << " at " << new_rReq->requested_time << endl;
                    
                }

                /* aux stat */
                new_info->in_time = system_time;

                l1_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line,
                                                                   dir_rep->data,
                                                                   new_info));
                l1_req->set_serialization_begin_time(system_time);
                l1_req->set_unset_dirty_on_write(true);
                l1_req->set_claim(true);
                l1_req->set_evict(true);
                l1_req->set_aux_info_for_coherence
                    (std::shared_ptr<cacheAuxInfoForCoherence>(new cacheAuxInfoForCoherence(UPDATE_FOR_DIR_REP)));

                if (l1_req->use_read_ports()) {
                    m_l1_read_req_schedule_q.push_back(entry);
                } else {
                    m_l1_write_req_schedule_q.push_back(entry);
                }

                entry->status = _CACHE_UPDATE_L1;
                ++it_addr;
                continue;
                /* TRANSITION */

            }
        } else if (check_to_send_rReq) {

            /* 1. after a read L1 hit, if no valid dir rep at the point */
            /* 2. after sending a cache rep for a eviction, if the new line is in timestamped */  
            /* 3. after renewing L1, right after a read L1 hit ( haven't checked whether to send a rReq after the hit ) */
            /* 4. after updating L1 without eviction, if the new line is in timestamped */

            mh_assert(m_cfg.renewal_type == _RENEWAL_SYNCHED);
            bool send_rReq = m_cfg.renewal_type != _RENEWAL_NEVER &&
                             l1_line_info->status == TIMESTAMPED &&
                             (m_cfg.renewal_threshold == 0 || *(l1_line_info->timestamp) - system_time < m_cfg.renewal_threshold);

            if (send_rReq && m_cache_renewal_table_vacancy > 0) { 
                cache_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_req->sender = m_id;
                cache_req->receiver = l1_line_info->home;
                cache_req->type = RREQ;
                cache_req->timestamp = l1_line_info->timestamp;
                cache_req->maddr = start_maddr;
                cache_req->sent = false;
                cache_req->per_mem_instr_stats = std::shared_ptr<privateSharedPTIStatsPerMemInstr>();
                cache_req->word_count = 0;
                cache_req->data = shared_array<uint32_t>();
                cache_req->birthtime = system_time;
                m_cache_req_schedule_q.push_back(entry);

                --m_cache_renewal_table_vacancy;
                ++m_cache_table_vacancy;
                ++m_available_core_ports;

                entry->short_latency_begin_time = system_time;
                entry->status = _CACHE_SEND_RREQ;
                ++it_addr;
                continue;
                /* TRANSITION */
            } else {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finishes serving a read on "
                          << core_req->maddr() << endl;
                if (stats_enabled() && per_mem_instr_stats) {
                    stats()->commit_per_mem_instr_stats(per_mem_instr_stats);
                }
                set_req_status(core_req, REQ_DONE);

                ++m_available_core_ports;
                ++m_cache_table_vacancy;
                m_cache_table.erase(it_addr++);
                continue;
                /* FINISHED */
            }

        }
    }
}

void privateSharedPTI::update_dir_table() {

    for (dirTable::iterator it_addr = m_dir_table.begin(); it_addr != m_dir_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        std::shared_ptr<dirTableEntry>& entry = it_addr->second;

        std::shared_ptr<coherenceMsg> & cache_req = entry->cache_req;
        std::shared_ptr<coherenceMsg> & bypassed_tReq = entry->bypassed_tReq;
        std::shared_ptr<cacheRequest> & l2_req = entry->l2_req;
        std::shared_ptr<coherenceMsg> & cache_rep = entry->cache_rep;
        std::shared_ptr<coherenceMsg> & dir_req = entry->dir_req;
        std::shared_ptr<coherenceMsg> & dir_rep = entry->dir_rep;
        std::shared_ptr<coherenceMsg> & empty_req = entry->empty_req;
        std::shared_ptr<dramctrlMsg> & dramctrl_req = entry->dramctrl_req;
        std::shared_ptr<dramctrlMsg> & dramctrl_rep = entry->dramctrl_rep;

        std::shared_ptr<cacheLine> l2_line = (l2_req)? l2_req->line_copy() : std::shared_ptr<cacheLine>();
        std::shared_ptr<cacheLine> l2_victim = (l2_req)? l2_req->line_to_evict_copy() : std::shared_ptr<cacheLine>();
        std::shared_ptr<dirCoherenceInfo> l2_line_info = (l2_line)? 
            static_pointer_cast<dirCoherenceInfo>(l2_line->coherence_info) : std::shared_ptr<dirCoherenceInfo>();
        std::shared_ptr<dirCoherenceInfo> l2_victim_info = (l2_victim)? 
            static_pointer_cast<dirCoherenceInfo>(l2_victim->coherence_info) : std::shared_ptr<dirCoherenceInfo>();
        std::shared_ptr<dirAuxInfoForCoherence> l2_aux_info = 
            static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence());

        std::shared_ptr<privateSharedPTIStatsPerMemInstr> & per_mem_instr_stats = entry->per_mem_instr_stats;
        std::shared_ptr<privateSharedPTIStatsPerMemInstr> dir_stat = 
            (per_mem_instr_stats)? per_mem_instr_stats->get_tentative_data(T_IDX_DIR) : 
                                   std::shared_ptr<privateSharedPTIStatsPerMemInstr>();

        if (entry->status == _DIR_L2_FOR_PREQ_OR_TREQ) {

            if (l2_req->status() == CACHE_REQ_NEW) {
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
            } else if (l2_req->status() == CACHE_REQ_HIT) {
                    std::shared_ptr<dirCoherenceInfo> prev_info = 
                    static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence())->previous_info;

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a L2 read HIT on " << cache_req->maddr << endl;
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> l2_stat = dir_stat->get_tentative_data(T_IDX_L2);
                        mh_assert(prev_info->status == TIMESTAMPED);
                        l2_stat->add_l2_ops(system_time - l2_req->operation_begin_time());
                        if (*(prev_info->max_timestamp) <= system_time) {
                            l2_stat->add_l2_hit_on_expired_T(l2_stat->total_cost(), 
                                                              (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ,
                                                              (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                            if (per_mem_instr_stats->is_read()) {
                                stats()->hit_on_expired_T_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                               PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                            } else {
                                stats()->hit_on_expired_T_for_write_instr_at_l2((cache_req->sender == m_id)? 
                                                                                PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                            }
                        } else {
                            mh_assert(cache_req->type == TREQ);
                            l2_stat->add_l2_hit_on_valid_T(dir_stat->total_cost(), 
                                                            (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ,
                                                            (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                            stats()->hit_on_valid_T_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                         PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                        }
                        dir_stat->commit_tentative_data(T_IDX_L2);
                    }
                } else if (per_mem_instr_stats) {
                    dir_stat->clear_tentative_data();
                }
                dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                if (l2_line_info->status == TIMESTAMPED) {
                    dir_rep->type = TREP;
                } else {
                    dir_rep->type = PREP;
                }
                dir_rep->timestamp = l2_line_info->max_timestamp;
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;
                dir_rep->requested_time = cache_req->birthtime;
                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
                ++it_addr;
                continue;
                /* TRANSITION */
            } else {

                if (!l2_line) {
                    /* True miss */
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a true L2 read miss on " 
                              << cache_req->maddr << endl;
                    if (stats_enabled()) {
                        if (per_mem_instr_stats) {
                                std::shared_ptr<privateSharedPTIStatsPerMemInstr> l2_stat = dir_stat->get_tentative_data(T_IDX_L2);
                            l2_stat->add_l2_ops(system_time - l2_req->serialization_begin_time());
                            l2_stat->add_l2_miss_true(l2_stat->total_cost(), 
                                                      (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ,
                                                      (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                            dir_stat->commit_tentative_data(T_IDX_L2);
                            if (per_mem_instr_stats->is_read()) {
                                stats()->miss_true_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                        PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                            } else {
                                stats()->miss_true_for_write_instr_at_l2((cache_req->sender == m_id)? 
                                                                         PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                            }
                        }
                    } else if (per_mem_instr_stats) {
                        dir_stat->clear_tentative_data();
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

                    entry->status = _DIR_SEND_DRAMCTRL_REQ;
                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else {

                        std::shared_ptr<dirCoherenceInfo> prev_info = 
                        static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence())->previous_info;

                    if (prev_info->status == TIMESTAMPED) {
                        /* P miss on valid T */
                        mh_assert(m_cfg.renewal_type != _RENEWAL_IDEAL);
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 miss for pReq due to valid timestamp on "
                            << cache_req->maddr << endl;
                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> l2_stat = dir_stat->get_tentative_data(T_IDX_L2);
                                l2_stat->add_l2_ops(system_time - l2_req->serialization_begin_time());
                                l2_stat->add_l2_miss_on_valid_T(l2_stat->total_cost(), 
                                                                PTI_STAT_PREQ,
                                                                (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                dir_stat->commit_tentative_data(T_IDX_L2);
                                if (per_mem_instr_stats->is_read()) {
                                    stats()->miss_on_valid_T_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                                  PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                } else {
                                    stats()->miss_on_valid_T_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                                  PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                }
                            }
                        } else if (per_mem_instr_stats) {
                            dir_stat->clear_tentative_data();
                        }

                        entry->block_or_inv_begin_time = system_time;
                        entry->blocked_line_info = l2_line_info;
                        entry->status = _DIR_WAIT_TIMESTAMP;
                        entry->substatus = _DIR_WAIT_TIMESTAMP__CACHE_REQ;
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else if (prev_info->owner != cache_req->sender) {
                        /* miss on other's P */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 miss for ";
                        if (cache_req->type == TREQ) {
                            mh_log(4) << "tReq ";
                        } else {
                            mh_log(4) << "pReq ";
                        }
                        mh_log(4) << "as line owned by " << prev_info->owner << " on " << cache_req->maddr << endl;
                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> l2_stat = dir_stat->get_tentative_data(T_IDX_L2);
                                l2_stat->add_l2_ops(system_time - l2_req->serialization_begin_time());
                                l2_stat->add_l2_miss_on_P(l2_stat->total_cost(), 
                                                          (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ,
                                                          (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                dir_stat->commit_tentative_data(T_IDX_L2);
                                if (per_mem_instr_stats->is_read()) {
                                    stats()->miss_on_P_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                            PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                } else {
                                    stats()->miss_on_P_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                            PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                }
                            }
                        } else if (per_mem_instr_stats) {
                            dir_stat->clear_tentative_data();
                        }

                        dir_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                        dir_req->sender = m_id;
                        dir_req->receiver = prev_info->owner;
                        dir_req->type = (cache_req->type == TREQ)? SWITCH_REQ : INV_REQ;
                        if (cache_req->type == TREQ) {
                            dir_req->type = SWITCH_REQ;
                            dir_req->timestamp = l2_line_info->max_timestamp;
                        } else {
                            dir_req->type = INV_REQ;
                        }
                        dir_req->word_count = 0;
                        dir_req->maddr = start_maddr;
                        dir_req->data = shared_array<uint32_t>();
                        dir_req->sent = false;
                        dir_req->per_mem_instr_stats = std::shared_ptr<privateSharedPTIStatsPerMemInstr>();
                        dir_req->birthtime = system_time;
                        m_dir_req_schedule_q.push_back(entry); /* mark it as from local */

                        entry->block_or_inv_begin_time = system_time;
                        entry->status = _DIR_SEND_DIR_REQ;
                        entry->substatus = _DIR_SEND_DIR_REQ__CACHE_REQ;
                        ++it_addr;
                        continue;
                        /* TRANSITION */

                    } else {
                        /* reorder */
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets an L2 miss for pReq - reorder detected on "
                            << cache_req->maddr << endl;

                        if (stats_enabled()) {
                            if (per_mem_instr_stats) {
                                    std::shared_ptr<privateSharedPTIStatsPerMemInstr> l2_stat = dir_stat->get_tentative_data(T_IDX_L2);
                                l2_stat->add_l2_ops(system_time - l2_req->serialization_begin_time());
                                l2_stat->add_l2_miss_reorder(l2_stat->total_cost(), 
                                                             (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ,
                                                             (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                dir_stat->commit_tentative_data(T_IDX_L2);
                                if (per_mem_instr_stats->is_read()) {
                                    stats()->miss_reorder_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                               PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                } else {
                                    stats()->miss_reorder_for_read_instr_at_l2((cache_req->sender == m_id)? 
                                                                               PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                                }
                            }
                        } else if (per_mem_instr_stats) {
                            dir_stat->clear_tentative_data();
                        }

                        entry->block_or_inv_begin_time = system_time;
                        entry->status = _DIR_WAIT_CACHE_REP;
                        entry->substatus = _DIR_WAIT_CACHE_REP__REORDER;
                        ++it_addr;
                        continue;
                        /* TRANSITION */
                    }

                }
            }

            ++it_addr;
            continue;

            /* _DIR_L2_FOR_PREQ_OR_TREQ */

        } else if (entry->status == _DIR_SEND_DRAMCTRL_REQ) {
            if (!dramctrl_req->sent) {
                m_dramctrl_req_schedule_q.push_back(make_tuple(false, entry));
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a DRAMCTRL request for "
                      << dramctrl_req->dram_req->maddr() << " to " << dramctrl_req->receiver << endl;
            
            if (stats_enabled()) {
                stats()->req_sent(PTI_STAT_DRAMCTRL_READ_REQ, (dramctrl_req->receiver ==  m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
            }

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
                dir_stat->add_rep_nas(system_time - dramctrl_rep->birthtime, PTI_STAT_DRAMCTRL_REP,
                                      (dramctrl_rep->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
            }

            std::shared_ptr<dirCoherenceInfo> new_info(new dirCoherenceInfo);

            new_info->status = PRIVATE; /* NOTE : default start : private */
            new_info->locked = false;
            new_info->owner = cache_req->sender;

            l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                               m_cfg.words_per_cache_line,
                                                               dramctrl_rep->dram_req->read(),
                                                               new_info));
            l2_req->set_serialization_begin_time(system_time);
            l2_req->set_unset_dirty_on_write(true);
            l2_req->set_claim(true);
            l2_req->set_evict(true);

            std::shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence(m_cfg));
            aux_info->core_id = cache_req->sender;
            aux_info->req_type = UPDATE_FOR_FEED;
            l2_req->set_aux_info_for_coherence(aux_info);

            if (l2_req->use_read_ports()) {
                m_l2_read_req_schedule_q.push_back(entry);
            } else {
                m_l2_write_req_schedule_q.push_back(entry);
            }

            entry->status = _DIR_L2_UPDATE;
            entry->substatus = _DIR_L2_UPDATE__DRAM_REP;
            ++it_addr;
            continue;
            /* TRANSITION */

            /* _DIR_WAIT_DRAMCTRL_REP */

        } else if (entry->status == _DIR_L2_UPDATE) {

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

            if (entry->substatus == _DIR_L2_UPDATE__DRAM_REP) {
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                            std::shared_ptr<privateSharedPTIStatsPerMemInstr> l2_stat = dir_stat->get_tentative_data(T_IDX_L2);
                        l2_stat->add_l2_ops(system_time - l2_req->serialization_begin_time());
                        l2_stat->add_l2_for_feed(dir_stat->total_cost(), 
                                                 (cache_req->type == PREQ)? PTI_STAT_PREQ : PTI_STAT_TREQ,
                                                 (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                        dir_stat->commit_tentative_data(T_IDX_L2);
                    }
                } else if (per_mem_instr_stats) {
                    dir_stat->clear_tentative_data();
                }

                if (l2_req->status() == CACHE_REQ_MISS) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] L2 feed failed on " << start_maddr << endl;

                    if (!l2_victim) {
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] L2 has no victim candidate for "
                                  << start_maddr << " , retrying" << endl;

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
                    } else {
                        mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] now evicting line " << l2_victim->start_maddr
                                  << " for " << start_maddr;
                        if (l2_victim_info->status == TIMESTAMPED) {
                            mh_log(4) << " in TIMESTAMPED mode until " << *(l2_victim_info->max_timestamp) << endl;
                        } else {
                            mh_log(4) << " in PRIVATE mode by invalidation " << endl;
                        }

                        empty_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);

                        empty_req->sender = m_id;
                        empty_req->receiver = m_id;
                        empty_req->type = EMPTY_REQ;
                        empty_req->word_count = m_cfg.words_per_cache_line;
                        empty_req->maddr = l2_victim->start_maddr;
                        empty_req->data = dramctrl_rep->dram_req->read();
                        empty_req->sent = false;
                        empty_req->replacing_info = l2_req->coherence_info_to_write();
                        empty_req->replacing_maddr = l2_req->maddr();
                        empty_req->is_empty_req_done = false;

                        empty_req->per_mem_instr_stats = per_mem_instr_stats;
                        empty_req->birthtime = system_time;

                        m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(FROM_LOCAL_CACHE, empty_req));

                        entry->status = _DIR_SEND_EMPTY_REQ_AND_WAIT;
                        ++it_addr;
                        continue;

                    }
                } else {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] L2 feed success on " << start_maddr << endl;
                    
                    if (!l2_victim || !l2_victim->data_dirty) {
                        /* no need to writeback */

                        if (stats_enabled() && l2_victim) {
                            stats()->evict_from_l2();
                            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] invalidated a clean line of "
                                << l2_victim->start_maddr << endl;
                        }

                        /* NOTE start from PREP */
                        dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                        dir_rep->sender = m_id;
                        dir_rep->receiver = cache_req->sender;
                        dir_rep->type = PREP;
                        dir_rep->timestamp = l2_line_info->max_timestamp;
                        dir_rep->word_count = m_cfg.words_per_cache_line;
                        dir_rep->maddr = start_maddr;
                        dir_rep->data = l2_line->data;
                        dir_rep->sent = false;
                        dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                        dir_rep->birthtime = system_time;
                        dir_rep->requested_time = cache_req->birthtime;

                        m_dir_rep_schedule_q.push_back(entry);

                        entry->status = _DIR_SEND_DIR_REP;
                        entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
                        ++it_addr;
                        continue;
                    } else {
                        /* need to writeback */

                        if (stats_enabled()) {
                            stats()->evict_from_l2();
                            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] invalidated a dirty line of "
                                      << l2_victim->start_maddr << " by an L2 feed" << endl;
                        }

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

                        m_dramctrl_req_schedule_q.push_back(make_tuple(false/*local*/, entry));
                        m_dramctrl_writeback_status[l2_victim->start_maddr] = entry;

                        entry->status = _DIR_SEND_DRAMCTRL_WRITEBACK;
                        entry->substatus = _DIR_SEND_DRAMCTRL_WRITEBACK__FEED;
                        ++it_addr;
                        continue;

                        /* TRANSITION */
                    }
                }
            } else if (entry->substatus == _DIR_L2_UPDATE__CACHE_REP) {

                mh_assert(l2_req->status() == CACHE_REQ_HIT && !l2_victim);

                if (stats_enabled()) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] updated a line by a ";
                    if (cache_rep->type == INV_REP) {
                        mh_log(4) << "invRep ";
                    } else {
                        mh_log(4) << "switchRep ";
                    }
                    mh_log(4) << " on " << cache_rep->maddr << endl;
                }

                if (cache_req) {
                    /* NOTE start from PREP */
                    dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                    dir_rep->sender = m_id;
                    dir_rep->receiver = cache_req->sender;
                    if (l2_line_info->status == TIMESTAMPED) {
                        dir_rep->type = TREP;
                    } else {
                        dir_rep->type = PREP;
                    }
                    dir_rep->timestamp = l2_line_info->max_timestamp;
                    dir_rep->word_count = m_cfg.words_per_cache_line;
                    dir_rep->maddr = start_maddr;
                    dir_rep->data = l2_line->data;
                    dir_rep->sent = false;
                    dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                    dir_rep->birthtime = system_time;
                    dir_rep->requested_time = cache_req->birthtime;
                    m_dir_rep_schedule_q.push_back(entry);

                    entry->status = _DIR_SEND_DIR_REP;
                    entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
                    ++it_addr;
                    continue;
                    /* TRANSITION */
                } else {

                    if (entry->using_cache_rep_exclusive_space) {
                        ++m_dir_table_vacancy_cache_rep_exclusive;
                    } else if (entry->using_empty_req_exclusive_space) {
                        ++m_dir_table_vacancy_empty_req_exclusive;
                    } else {
                        ++m_dir_table_vacancy_shared;
                    }
                    m_dir_table.erase(it_addr++);
                    continue;
                    /* FINISH */
                }

            }
        } else if (entry->status == _DIR_L2_FOR_RREQ) {

            if (l2_req->status() == CACHE_REQ_NEW) {
                if (l2_req->use_read_ports()) {
                    m_l2_read_req_schedule_q_for_renewal.push_back(entry);
                } else {
                    m_l2_write_req_schedule_q_for_renewal.push_back(entry);
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

                    std::shared_ptr<dirCoherenceInfo> prev_info = 
                    static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence())->previous_info;

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] processed rReq from " << cache_req->sender 
                          << " and extended timestamp ( "
                          << *(prev_info->max_timestamp) << " to " << *(l2_line_info->max_timestamp) << " ) " << endl;

                dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                dir_rep->type = RREP;
                dir_rep->timestamp = std::shared_ptr<uint64_t>(new uint64_t(*(l2_line_info->max_timestamp))); 
                if (static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence())->need_to_send_block) {
                    dir_rep->word_count = m_cfg.words_per_cache_line;
                } else {
                    dir_rep->word_count = 0;
                }
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;
                dir_rep->requested_time = cache_req->birthtime;
                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                entry->substatus = _DIR_SEND_DIR_REP__RREP;
                ++it_addr;
                continue;

            } else {
                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] processed and discarded a rReq on " << start_maddr 
                          << " from " << cache_req->sender << endl;
                ++m_dir_renewal_table_vacancy;
                m_dir_table.erase(it_addr++);
                continue;
                /* FINISH */
            }

        } else if (entry->status == _DIR_SEND_BYPASS_DIR_REP) {
            if (!dir_rep->sent) {
                m_dir_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a bypassed tRep on "
                      << dir_rep->maddr << " to " << dir_rep->receiver << endl;
            if (stats_enabled()) {
                stats()->rep_sent(PTI_STAT_TREP, (dir_rep->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
            }

            entry->bypassed_tReq = std::shared_ptr<coherenceMsg>();
 
            std::shared_ptr<dirCoherenceInfo> prev_info = 
                static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence())->previous_info;

            if (*(prev_info->max_timestamp) > system_time) {
                entry->status = _DIR_WAIT_TIMESTAMP;
                ++it_addr;
                continue;
                /* TRANSITION */
            } else {
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        dir_stat->add_block_cost(system_time - entry->block_or_inv_begin_time, 
                                                 (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ);
                    }
                }

                dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                if (l2_line_info->status == TIMESTAMPED) {
                    dir_rep->type = TREP;
                } else {
                    dir_rep->type = PREP;
                }
                dir_rep->timestamp = l2_line_info->max_timestamp;
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;
                dir_rep->requested_time = cache_req->birthtime;
                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
                ++it_addr;
                continue;
                /* TRANSITION */

            }

        } else if (entry->status == _DIR_WAIT_TIMESTAMP) {

            mh_assert(m_cfg.renewal_type != _RENEWAL_IDEAL);

            std::shared_ptr<dirCoherenceInfo> prev_info = 
                static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence())->previous_info;

            if (bypassed_tReq) {
                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] gets a local bypass HIT on "
                          << bypassed_tReq->maddr << endl;
                if (stats_enabled() && bypassed_tReq->per_mem_instr_stats) {
                    bypassed_tReq->per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->
                        add_bypass(system_time - entry->bypass_begin_time, 
                                   (bypassed_tReq->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                }

                dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = bypassed_tReq->sender;
                dir_rep->type = TREP;
                dir_rep->timestamp = std::shared_ptr<uint64_t>(new uint64_t(*(prev_info->max_timestamp)));
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = bypassed_tReq->per_mem_instr_stats;
                dir_rep->birthtime = system_time;
                dir_rep->requested_time = bypassed_tReq->birthtime;
                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_BYPASS_DIR_REP;
                ++it_addr;
                continue;
                /* TRANSITION */
            }

            if (*(prev_info->max_timestamp) > system_time) {
                ++it_addr;
                continue;
                /* SPIN */
            }

            if (entry->substatus == _DIR_WAIT_TIMESTAMP__CACHE_REQ) {
                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        dir_stat->add_block_cost(system_time - entry->block_or_inv_begin_time, 
                                                 (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ);
                    }
                }

                dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                if (l2_line_info->status == TIMESTAMPED) {
                    dir_rep->type = TREP;
                } else {
                    dir_rep->type = PREP;
                }
                dir_rep->timestamp = l2_line_info->max_timestamp;
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;
                dir_rep->requested_time = cache_req->birthtime;
                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
                ++it_addr;
                continue;
                /* TRANSITION */

            } else {

                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        dir_stat->add_block_cost(system_time - entry->block_or_inv_begin_time, PTI_STAT_EMPTY_REQ);
                    }
                }

                if (l2_aux_info->is_replaced_line_dirty) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a timestamped line of "
                              << start_maddr << " by an empty req without invalidation but with writeback" << endl;

                    dramctrl_req = std::shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = start_maddr;

                    dramctrl_req->dram_req = std::shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                     DRAM_REQ_WRITE,
                                                                                     m_cfg.words_per_cache_line,
                                                                                     l2_aux_info->replaced_line));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false/*local*/, entry));
                    m_dramctrl_writeback_status[start_maddr] = entry;

                    entry->status = _DIR_SEND_DRAMCTRL_WRITEBACK;
                    entry->substatus = _DIR_SEND_DRAMCTRL_WRITEBACK__EVICTION;
                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a line of "
                              << start_maddr << " by an empty req without invalidation/writeback" << endl;
                    empty_req->is_empty_req_done = true;

                    if (entry->using_cache_rep_exclusive_space) {
                        ++m_dir_table_vacancy_cache_rep_exclusive;
                    } else if (entry->using_empty_req_exclusive_space) {
                        ++m_dir_table_vacancy_empty_req_exclusive;
                    } else {
                        ++m_dir_table_vacancy_shared;
                    }
                    m_dir_table.erase(it_addr++);
                    continue;
                    /* FINISH */
                }
            }

            /* _DIR_WAIT_TIMESTAMP */

        } else if (entry->status == _DIR_SEND_DIR_REP) {
            if (!dir_rep->sent) {
                m_dir_rep_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a ";
            if (dir_rep->type == TREP) {
                mh_log(4) << "tRep ";
            } else if (dir_rep->type == PREP) {
                mh_log(4) << "pRep ";
            } else {
                mh_log(4) << "rRep ";
            }
            mh_log(4) << " on " << dir_rep->maddr << " to " << dir_rep->receiver << endl;

            if (stats_enabled()) {
                ptiRepType rep_type = (dir_rep->type == TREP)? PTI_STAT_TREP :
                                      (dir_rep->type == PREP)? PTI_STAT_PREP :
                                      PTI_STAT_RREP;
                stats()->rep_sent(rep_type, (dir_rep->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
            }
            
            if (entry->substatus == _DIR_SEND_DIR_REP__TREP_PREP) {
                if (entry->using_cache_rep_exclusive_space) {
                    ++m_dir_table_vacancy_cache_rep_exclusive;
                } else if (entry->using_empty_req_exclusive_space) {
                    ++m_dir_table_vacancy_empty_req_exclusive;
                } else {
                    ++m_dir_table_vacancy_shared;
                }
                m_dir_table.erase(it_addr++);
                continue;
                /* FINISH */
            } else {
                mh_assert(entry->substatus == _DIR_SEND_DIR_REP__RREP);
                ++m_dir_renewal_table_vacancy;
                m_dir_table.erase(it_addr++);
                continue;
                /* FINISH */
            }

        } else if (entry->status == _DIR_SEND_EMPTY_REQ_AND_WAIT) {

            if (!empty_req->sent) {
                m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(FROM_LOCAL_CACHE, empty_req));
                ++it_addr;
                continue;
                /* SPIN */
            } 
            if (!empty_req->is_empty_req_done) {
                /* eviction is in progress */
                ++it_addr;
                continue;
                /* SPIN */
            }
            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] done evicting " << empty_req->maddr 
                      << " for " << start_maddr << endl;
            if (stats_enabled()) {
                /* not precisely in timing but correct */
                stats()->req_sent(PTI_STAT_EMPTY_REQ, PTI_STAT_LOCAL);
            }

            dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
            dir_rep->sender = m_id;
            dir_rep->receiver = cache_req->sender;
            std::shared_ptr<dirCoherenceInfo> line_info = static_pointer_cast<dirCoherenceInfo>(empty_req->replacing_info);
            dir_rep->type = (line_info->status == PRIVATE)? PREP : TREP;
            dir_rep->timestamp = line_info->max_timestamp; 
            dir_rep->word_count = m_cfg.words_per_cache_line;
            dir_rep->maddr = start_maddr;
            dir_rep->data = dramctrl_rep->dram_req->read();
            dir_rep->sent = false;
            dir_rep->per_mem_instr_stats = per_mem_instr_stats;
            dir_rep->birthtime = system_time;
            dir_rep->requested_time = cache_req->birthtime;
            m_dir_rep_schedule_q.push_back(entry);

            entry->status = _DIR_SEND_DIR_REP;
            entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
            ++it_addr;
            continue;
        } else if (entry->status == _DIR_SEND_DIR_REQ) {
            if (!dir_req->sent) {
                m_dir_req_schedule_q.push_back(entry);
                ++it_addr;
                continue;
                /* SPIN */
            }

            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] sent a ";
            if (dir_req->type == INV_REQ) {
                mh_log(4) << "invReq ";
            } else {
                mh_log(4) << "switchReq ";
            }
            mh_log(4) << " on " << dir_req->maddr << " to " << dir_req->receiver << endl;

            if (stats_enabled()) {
                stats()->req_sent((dir_req->type == INV_REQ)? PTI_STAT_INV_REQ : PTI_STAT_SWITCH_REQ, 
                                  (dir_req->receiver == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
            }

            entry->status = _DIR_WAIT_CACHE_REP;
            if (entry->substatus == _DIR_SEND_DIR_REQ__CACHE_REQ) {
                entry->substatus = _DIR_WAIT_CACHE_REP__CACHE_REQ;
            } else {
                entry->substatus = _DIR_WAIT_CACHE_REP__EMPTY_REQ;
            }

            ++it_addr;
            continue;
        } else if (entry->status == _DIR_WAIT_CACHE_REP) {

            if (!cache_rep) {
                ++it_addr;
                continue;
                /* SPIN */
            }
            mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] received a ";
            if (cache_rep->type == INV_REP) {
                mh_log(4) << "invRep ";
            } else {
                mh_log(4) << "switchRep ";
            }
            mh_log(4) << " on " << cache_rep->maddr << " from " << cache_rep->receiver << endl;

            if (stats_enabled() && per_mem_instr_stats) {
                if (entry->substatus == _DIR_WAIT_CACHE_REP__CACHE_REQ) {
                    dir_stat->add_block_cost(system_time - entry->block_or_inv_begin_time, 
                                             (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ);
                } else if (entry->substatus == _DIR_WAIT_CACHE_REP__REORDER) {
                    dir_stat->add_reorder_cost(system_time - entry->block_or_inv_begin_time, 
                                               (cache_req->type == TREQ)? PTI_STAT_TREQ : PTI_STAT_PREQ,
                                               (cache_req->sender == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
                } else {
                    /* __EMPTY_REQ */
                    dir_stat->add_inv_cost(system_time - entry->block_or_inv_begin_time, 
                                           PTI_STAT_EMPTY_REQ, PTI_STAT_LOCAL);
                }
            }

            if (entry->substatus == _DIR_WAIT_CACHE_REP__EMPTY_REQ) {
                if (cache_rep->word_count > 0) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a timestamped line of "
                              << start_maddr << " by an empty req with invalidation and writeback" << endl;

                    dramctrl_req = std::shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = start_maddr;

                    dramctrl_req->dram_req = std::shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                     DRAM_REQ_WRITE,
                                                                                     m_cfg.words_per_cache_line,
                                                                                     cache_rep->data));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false/*local*/, entry));
                    m_dramctrl_writeback_status[start_maddr] = entry;

                    entry->status = _DIR_SEND_DRAMCTRL_WRITEBACK;
                    entry->substatus = _DIR_SEND_DRAMCTRL_WRITEBACK__EVICTION;
                    ++it_addr;
                    continue;
                    /* TRANSITION */


                } else {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a line of "
                              << start_maddr << " by an empty req with invalidation, without writeback" << endl;
                    empty_req->is_empty_req_done = true;

                    if (entry->using_cache_rep_exclusive_space) {
                        ++m_dir_table_vacancy_cache_rep_exclusive;
                    } else if (entry->using_empty_req_exclusive_space) {
                        ++m_dir_table_vacancy_empty_req_exclusive;
                    } else {
                        ++m_dir_table_vacancy_shared;
                    }
                    m_dir_table.erase(it_addr++);
                    continue;
                    /* FINISH */

                }

            } else if (cache_rep->word_count > 0) {

                l2_req = std::shared_ptr<cacheRequest>(new cacheRequest(cache_rep->maddr, CACHE_REQ_UPDATE,
                                                                   cache_rep->word_count,
                                                                   cache_rep->data
                                                                  ));
                l2_req->set_serialization_begin_time(system_time);
                l2_req->set_unset_dirty_on_write(false);
                l2_req->set_claim(false);
                l2_req->set_evict(false);

                std::shared_ptr<dirAuxInfoForCoherence> aux_info(new dirAuxInfoForCoherence(m_cfg));
                aux_info->core_id = cache_rep->sender;
                aux_info->req_type = UPDATE_FOR_CACHE_REP_FOR_REQ;
                l2_req->set_aux_info_for_coherence(aux_info);

                entry->status = _DIR_L2_UPDATE;
                entry->substatus = _DIR_L2_UPDATE__CACHE_REP;
                ++it_addr;
                continue;
                /* TRANSITION */

            } else {

                dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                if (l2_line_info->status == TIMESTAMPED) {
                    dir_rep->type = TREP;
                } else {
                    dir_rep->type = PREP;
                }
                dir_rep->timestamp = l2_line_info->max_timestamp;
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;
                dir_rep->requested_time = cache_req->birthtime;
                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
                ++it_addr;
                continue;
                /* TRANSITION */
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
                dir_stat->add_req_nas(system_time - dramctrl_req->birthtime, PTI_STAT_DRAMCTRL_WRITE_REQ, 
                                      (m_dramctrl_location == m_id)? PTI_STAT_LOCAL : PTI_STAT_REMOTE);
            }

            if (entry->substatus == _DIR_SEND_DRAMCTRL_WRITEBACK__FEED) {
                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] written back sent for address "
                          << l2_victim->start_maddr << endl;

                dir_rep = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = cache_req->sender;
                dir_rep->type = (l2_line_info->status == TIMESTAMPED)? TREP : PREP;
                dir_rep->timestamp = l2_line_info->max_timestamp;
                dir_rep->word_count = m_cfg.words_per_cache_line;
                dir_rep->maddr = start_maddr;
                dir_rep->data = l2_line->data;
                dir_rep->sent = false;
                dir_rep->per_mem_instr_stats = per_mem_instr_stats;
                dir_rep->birthtime = system_time;

                m_dir_rep_schedule_q.push_back(entry);

                entry->status = _DIR_SEND_DIR_REP;
                entry->substatus = _DIR_SEND_DIR_REP__TREP_PREP;
                ++it_addr;
                continue;
            } else {
                mh_log(4) << "[DRAMCTRL " << m_id << " @ " << system_time << " ] written back sent for address "
                          << l2_req->maddr() << " for empty req " << endl;

                empty_req->is_empty_req_done = true;

                if (entry->using_cache_rep_exclusive_space) {
                    ++m_dir_table_vacancy_cache_rep_exclusive;
                } else if (entry->using_empty_req_exclusive_space) {
                    ++m_dir_table_vacancy_empty_req_exclusive;
                } else {
                    ++m_dir_table_vacancy_shared;
                }
                m_dir_table.erase(it_addr++);
                continue;
                /* FINISH */

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
            if (stats_enabled()) {
                stats()->evict_from_l2();
                if (per_mem_instr_stats) {
                    dir_stat->get_tentative_data(T_IDX_L2)->add_l2_ops(system_time - l2_req->serialization_begin_time());
                    dir_stat->get_tentative_data(T_IDX_L2)->add_l2_for_emptyReq(dir_stat->get_tentative_data(T_IDX_L2)->total_cost());
                    dir_stat->commit_tentative_data(T_IDX_L2);
                } else {
                    dir_stat->clear_tentative_data();
                }
            }

            mh_assert(l2_req->status() == CACHE_REQ_HIT) ;

            std::shared_ptr<dirCoherenceInfo> prev_info = 
                static_pointer_cast<dirAuxInfoForCoherence>(l2_req->aux_info_for_coherence())->previous_info;

            if (prev_info->status == PRIVATE) {

                mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] need to invalidate to evict a line of "
                          << start_maddr << endl;

                /* cache the current directory */
                dir_req = std::shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_req->sender = m_id;
                dir_req->receiver = prev_info->owner;
                dir_req->type = INV_REQ;
                dir_req->type = INV_REQ;
                dir_req->word_count = 0;
                dir_req->maddr = start_maddr;
                dir_req->data = shared_array<uint32_t>();
                dir_req->sent = false;
                dir_req->per_mem_instr_stats = std::shared_ptr<privateSharedPTIStatsPerMemInstr>();
                dir_req->birthtime = system_time;
                m_dir_req_schedule_q.push_back(entry); /* mark it as from local */

                entry->block_or_inv_begin_time = system_time;
                entry->status = _DIR_SEND_DIR_REQ;
                entry->substatus = _DIR_SEND_DIR_REQ__EMPTY_REQ;
                ++it_addr;
                continue;
                /* TRANSITION */

            } else {
                /* timestamped */
                if (*(prev_info->max_timestamp) > system_time) {
                    entry->status = _DIR_WAIT_TIMESTAMP;
                    entry->substatus = _DIR_WAIT_TIMESTAMP__EMPTY_REQ;
                    ++it_addr;
                    continue;
                    /* TRANSITION */
                }

                if (stats_enabled()) {
                    if (per_mem_instr_stats) {
                        dir_stat->add_block_cost(system_time - entry->block_or_inv_begin_time, PTI_STAT_EMPTY_REQ);
                    }
                }

                if (l2_aux_info->is_replaced_line_dirty) {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a timestamped line of "
                              << start_maddr << " by an empty req without invalidation but with writeback" << endl;

                    dramctrl_req = std::shared_ptr<dramctrlMsg>(new dramctrlMsg);
                    dramctrl_req->sender = m_id;
                    dramctrl_req->receiver = m_dramctrl_location;
                    dramctrl_req->maddr = start_maddr;

                    dramctrl_req->dram_req = std::shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                     DRAM_REQ_WRITE,
                                                                                     m_cfg.words_per_cache_line,
                                                                                     l2_aux_info->replaced_line));
                    dramctrl_req->sent = false;
                    dramctrl_req->birthtime = system_time;
                    dramctrl_req->per_mem_instr_stats = per_mem_instr_stats;

                    m_dramctrl_req_schedule_q.push_back(make_tuple(false/*local*/, entry));
                    m_dramctrl_writeback_status[start_maddr] = entry;

                    entry->status = _DIR_SEND_DRAMCTRL_WRITEBACK;
                    entry->substatus = _DIR_SEND_DRAMCTRL_WRITEBACK__EVICTION;
                    ++it_addr;
                    continue;
                    /* TRANSITION */

                } else {
                    mh_log(4) << "[L2 " << m_id << " @ " << system_time << " ] evicted a line of "
                              << start_maddr << " by an empty req without invalidation/writeback" << endl;
                    empty_req->is_empty_req_done = true;

                    if (entry->using_cache_rep_exclusive_space) {
                        ++m_dir_table_vacancy_cache_rep_exclusive;
                    } else if (entry->using_empty_req_exclusive_space) {
                        ++m_dir_table_vacancy_empty_req_exclusive;
                    } else {
                        ++m_dir_table_vacancy_shared;
                    }
                    m_dir_table.erase(it_addr++);
                    continue;
                    /* FINISH */
                }
            }

            /* _DIR_L2_FOR_EMPTY_REQ */

        } else {
            ++it_addr;
            continue;
        }
    }
 
}

void privateSharedPTI::update_dramctrl_work_table() {

    for (dramctrlTable::iterator it_addr = m_dramctrl_work_table.begin(); it_addr != m_dramctrl_work_table.end(); ) {
            std::shared_ptr<dramctrlTableEntry>& entry = it_addr->second;
            std::shared_ptr<dramctrlMsg>& dramctrl_req = entry->dramctrl_req;
            std::shared_ptr<dramctrlMsg>& dramctrl_rep = entry->dramctrl_rep;
            std::shared_ptr<privateSharedPTIStatsPerMemInstr>& per_mem_instr_stats = entry->per_mem_instr_stats;

        /* only reads are in the table */
        if (dramctrl_req->dram_req->status() == DRAM_REQ_DONE) {

            if (!dramctrl_rep) {
                if (stats_enabled() && per_mem_instr_stats) {
                        std::shared_ptr<privateSharedPTIStatsPerMemInstr> dir_stat = per_mem_instr_stats->get_tentative_data(T_IDX_DIR);
                    //per_mem_instr_stats->get_tentative_data(T_IDX_DIR)->add_dram_ops(system_time - entry->operation_begin_time);
                    dir_stat->add_dram_ops(system_time - entry->operation_begin_time);
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

void privateSharedPTI::accept_incoming_messages() {

    /* Directory requests and replies (from the network) */
    while (m_core_receive_queues[MSG_DIR_REQ_REP]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_DIR_REQ_REP]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_REMOTE_DIR, data_msg));
        /* only one message a time (otherwise out-of-order reception may happen) */
        break;
    }
    while (m_core_receive_queues[MSG_DIR_RREP]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_DIR_RREP]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_cache_table_entry_schedule_q.push_back(make_tuple(FROM_REMOTE_DIR_RREP, data_msg));
        /* only one message a time (otherwise out-of-order reception may happen) */
        break;
    }

    /* Cache requests (from the network) */
    while (m_core_receive_queues[MSG_CACHE_REQ]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_REQ]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(FROM_REMOTE_CACHE_REQ, data_msg));
        break;
    }
    while (m_core_receive_queues[MSG_CACHE_PREQ]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_PREQ]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_dir_table_entry_for_req_schedule_q.push_back(make_tuple(FROM_REMOTE_CACHE_PREQ, data_msg));
        break;
    }
    while (m_core_receive_queues[MSG_CACHE_RREQ]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_RREQ]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_dir_table_entry_for_renewal_schedule_q.push_back(make_tuple(FROM_REMOTE_CACHE_RREQ, data_msg));
        break;
    }
       
    /* Cache replies (from the network) */
    while (m_core_receive_queues[MSG_CACHE_REP]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_REP]->front();
            std::shared_ptr<coherenceMsg> data_msg = static_pointer_cast<coherenceMsg>(msg->content);
        m_new_dir_table_entry_for_cache_rep_schedule_q.push_back(make_tuple(true/* is remote */, data_msg));
        break;
    }

    /* DRAMCTRL requests and replies from the network */
    if (m_core_receive_queues[MSG_DRAMCTRL_REQ]->size()) {
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REQ]->front();
            std::shared_ptr<dramctrlMsg> dram_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        m_dramctrl_req_schedule_q.push_back(make_tuple(true/* is remote */, dram_msg));
    }

    if (m_core_receive_queues[MSG_DRAMCTRL_REP]->size()) {
        /* note: no replies for DRAM writes */
            std::shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAMCTRL_REP]->front();
            std::shared_ptr<dramctrlMsg> dramctrl_msg = static_pointer_cast<dramctrlMsg>(msg->content);
        maddr_t start_maddr = dramctrl_msg->dram_req->maddr(); /* always access by a cache line */
        mh_assert(m_dir_table.count(start_maddr) > 0 &&
                  m_dir_table[dramctrl_msg->maddr]->status == _DIR_WAIT_DRAMCTRL_REP);
        m_dir_table[start_maddr]->dramctrl_rep = dramctrl_msg;
        m_core_receive_queues[MSG_DRAMCTRL_REP]->pop();
    }

}

privateSharedPTI::rReqScheduleQueue::rReqScheduleQueue(bool do_retry, const uint64_t& t) 
    : system_time(t), m_do_retry(do_retry) {}

privateSharedPTI::rReqScheduleQueue::~rReqScheduleQueue() {}

void privateSharedPTI::rReqScheduleQueue::set(std::shared_ptr<coherenceMsg> rReq) {
    
    m_book[rReq->maddr] = rReq;
    vector<std::shared_ptr<coherenceMsg> >::reverse_iterator rit;
    for (rit = m_schedule.rbegin(); rit < m_schedule.rend(); ++rit) {
        if ((*rit)->requested_time <= rReq->requested_time) {
            break;
        }
    }
    m_schedule.insert(rit.base(), rReq);

}

void privateSharedPTI::rReqScheduleQueue::remove(maddr_t addr) {
    if (m_book.count(addr)) {
        m_book.erase(addr);
    }
}

vector<std::shared_ptr<privateSharedPTI::coherenceMsg> > privateSharedPTI::rReqScheduleQueue::on_due() {

    vector<std::shared_ptr<coherenceMsg> > ret;

    for (vector<std::shared_ptr<coherenceMsg> >::iterator it = m_schedule.begin(); it != m_schedule.end(); ) {
        if ((*it)->requested_time > system_time) {
            break;
        }
        if (m_book.count((*it)->maddr) == 0 || m_book[(*it)->maddr] != *it) {
            it = m_schedule.erase(it);
            continue;
        }
        if (!m_do_retry && (*it)->requested_time < system_time) {
            it = m_schedule.erase(it);
            continue;
        }
        ret.push_back(*it);
        ++it;
    }
    return ret;

}


