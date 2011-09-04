// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSI.hpp"
#include "messages.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>

/* 64-bit address */

#define DEBUG
#undef DEBUG

#define PRINT_PROGRESS
//#undef PRINT_PROGRESS

#ifdef DEBUG
#define mh_log(X) cerr
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

static shared_ptr<void> cache_copy_coherence_info(shared_ptr<void> source) { 
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> ret
        (new privateSharedMSI::cacheCoherenceInfo(*static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(source)));
    return ret;
}

static shared_ptr<void> directory_copy_coherence_info(shared_ptr<void> source) {
    shared_ptr<privateSharedMSI::directoryCoherenceInfo> ret
        (new privateSharedMSI::directoryCoherenceInfo(*static_pointer_cast<privateSharedMSI::directoryCoherenceInfo>(source)));
    return ret;
}

static bool cache_is_hit(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(line.coherence_info);
    switch (req->request_type()) {
    case CACHE_REQ_READ:
        return info->status == privateSharedMSI::SHARED || info->status == privateSharedMSI::MODIFIED;
    case CACHE_REQ_WRITE:
        return info->status == privateSharedMSI::MODIFIED;
    case CACHE_REQ_INVALIDATE:
        return info->status != privateSharedMSI::PENDING;
    case CACHE_REQ_UPDATE:
        return true;
    default:
        mh_assert(false);
        return false;
    }
}

static void directory_reserve_line(cacheLine &line) { 
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<privateSharedMSI::directoryCoherenceInfo>(new privateSharedMSI::directoryCoherenceInfo);
    }
    return; 
}

static void cache_reserve_line(cacheLine &line) { 
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<privateSharedMSI::cacheCoherenceInfo>(new privateSharedMSI::cacheCoherenceInfo);
    }
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(line.coherence_info);
    info->status = privateSharedMSI::PENDING;
    return; 
}

static bool cache_can_evict_line(cacheLine &line, const uint64_t &system_time) {
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(line.coherence_info);
    return info->status != privateSharedMSI::PENDING;
}

static bool directory_can_evict_line(cacheLine &line, const uint64_t &syste_time) { 
    shared_ptr<privateSharedMSI::directoryCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::directoryCoherenceInfo>(line.coherence_info);
    return info->directory.empty();
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
    m_l1_work_table_vacancy(cfg.l1_work_table_size),
    m_l2_work_table_vacancy_shared(cfg.l2_work_table_size_shared),
    m_l2_work_table_vacancy_replies(cfg.l2_work_table_size_replies),
    m_l2_work_table_vacancy_evict(cfg.l2_work_table_size_evict), 
    m_available_core_ports(cfg.num_local_core_ports)
{
    if (m_cfg.bytes_per_flit == 0) throw err_bad_shmem_cfg("flit size must be non-zero.");
    if (m_cfg.words_per_cache_line == 0) throw err_bad_shmem_cfg("cache line size must be non-zero.");
    if (m_cfg.lines_in_l1 == 0) throw err_bad_shmem_cfg("privateSharedMSI/MESI : L1 size must be non-zero.");
    if (m_cfg.lines_in_l2 == 0) throw err_bad_shmem_cfg("privateSharedMSI/MESI : L2 size must be non-zero.");
    if (m_cfg.l1_work_table_size <= m_cfg.num_local_core_ports) 
        throw err_bad_shmem_cfg("privateSharedMSI/MESI : L1 work table size must be greater than local core ports.");
    if (m_cfg.l2_work_table_size_replies == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI/MESI : L2 work table must have non-zero space for cache replies.");
    if (m_cfg.l2_work_table_size_evict == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI/MESI : L2 work table must have non-zero space for line eviction.");
    if (m_cfg.l2_work_table_size_shared == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI/MESI : L2 work table must be non-zero.");
    if (m_cfg.l1_work_table_size == 0) 
        throw err_bad_shmem_cfg("privateSharedMSI/MESI : L1 work table must be non-zero.");

    m_l1 = new cache(1, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l1, cfg.l1_associativity, 
                     cfg.l1_replacement_policy, 
                     cfg.l1_hit_test_latency, cfg.l1_num_read_ports, cfg.l1_num_write_ports);
    m_l2 = new cache(2, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l2, cfg.l2_associativity, 
                     cfg.l2_replacement_policy,
                     cfg.l2_hit_test_latency, cfg.l2_num_read_ports, cfg.l2_num_write_ports);

    m_l1->set_helper_copy_coherence_info(&cache_copy_coherence_info);
    m_l1->set_helper_is_hit(&cache_is_hit);
    m_l1->set_helper_reserve_line(&cache_reserve_line);
    m_l1->set_helper_can_evict_line(&cache_can_evict_line);

    m_l2->set_helper_copy_coherence_info(&directory_copy_coherence_info);
    m_l2->set_helper_reserve_line(&directory_reserve_line);
    m_l2->set_helper_can_evict_line(&directory_can_evict_line);

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

    m_core_port_schedule_q.push_back(req);

}

void privateSharedMSI::tick_positive_edge() {
    /* schedule and make requests */
#ifdef PRINT_PROGRESS
    static uint64_t last_served[64];
    if (system_time % 100000 == 0) {
        cerr << "[MEM " << m_id << " @ " << system_time << " ]";
        if (stats_enabled()) {
            cerr << " total served : " << stats()->total_served();
            cerr << " since last : " << stats()->total_served() - last_served[m_id];
            last_served[m_id] = stats()->total_served();
        }
        cerr << " in L1 work table : " << m_l1_work_table.size() << " in L2 work table : " << m_l2_work_table.size() << endl;
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

    l1_work_table_update();

    l2_work_table_update();

    dram_work_table_update();

}

void privateSharedMSI::l1_work_table_update() {
    for (toL1Table::iterator it_addr = m_l1_work_table.begin(); it_addr != m_l1_work_table.end(); ) {
        maddr_t start_maddr = it_addr->first;
        shared_ptr<toL1Entry> entry = it_addr->second;
        shared_ptr<memoryRequest> core_req = entry->core_req;
        shared_ptr<cacheRequest> l1_req = entry->l1_req;
        shared_ptr<catRequest> cat_req = entry->cat_req;
        shared_ptr<coherenceMsg> dir_req = entry->dir_req;
        shared_ptr<coherenceMsg> dir_rep = entry->dir_rep;
        shared_ptr<coherenceMsg> cache_req = entry->cache_req;
        shared_ptr<coherenceMsg> cache_rep = entry->cache_rep;

        shared_ptr<cacheLine> line = l1_req ? l1_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> victim = l1_req ? l1_req->victim_line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheCoherenceInfo> line_info = line ? 
            static_pointer_cast<cacheCoherenceInfo>(line->coherence_info) : 
            shared_ptr<cacheCoherenceInfo>();
        shared_ptr<cacheCoherenceInfo> victim_info = victim ? 
            static_pointer_cast<cacheCoherenceInfo>(victim->coherence_info) : 
            shared_ptr<cacheCoherenceInfo>();

        //mh_log(4) << "## L1 table " << m_id << " @ " << system_time << " on address " << start_maddr << " state " << entry->status << endl;

        if (entry->status == _L1_WORK_READ_L1) {
            if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            if (l1_req->milestone_time() != UINT64_MAX) {
                if (stats_enabled()) {
                    stats()->add_l1_action_cost(system_time - l1_req->milestone_time());
                }
            }

            if (dir_req) {
                /* entry for a directory request */
                if (l1_req->status() == CACHE_REQ_MISS) {
                    /* the line was already invalidated and a cache reply must have been sent. */
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] discarded a directory request for "
                        << start_maddr << " as it was already invalidated." << endl;
                    ++m_l1_work_table_vacancy;
                    m_l1_work_table.erase(it_addr++);
                    continue;
                } else {
                    /* a hit */
                    uint32_t dir_home = line_info->directory_home;
                    cache_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                    cache_rep->sender = m_id;
                    cache_rep->receiver = dir_home;

                    /* cost breakdown study */
                    cache_rep->milestone_time = UINT64_MAX; /* will not count */

                    uint32_t data_size = 0;
                    switch (dir_req->type) {
                    case INV_REQ:
                        cache_rep->type = INV_REP;
                        if (stats_enabled()) {
                            stats()->add_invreq_replied();
                            stats()->add_invrep();
                        }
                        break;
                    case FLUSH_REQ:
                        cache_rep->type = FLUSH_REP;
                        if (!m_cfg.use_mesi || line->dirty) {
                            data_size = m_cfg.words_per_cache_line * 4;
                        }
                        if (stats_enabled()) {
                            stats()->add_flushreq_replied();
                            stats()->add_flushrep();
                        }
                        break;
                    case WB_REQ:
                        cache_rep->type = WB_REP;
                        if (!m_cfg.use_mesi || line->dirty) {
                            data_size = m_cfg.words_per_cache_line * 4;
                        }
                        if (stats_enabled()) {
                            stats()->add_wbreq_replied();
                            stats()->add_wbrep();
                        }
                        break;
                    default:
                        mh_assert(false);
                        break;
                    }
                    cache_rep->maddr = start_maddr;
                    cache_rep->data = line->data;
                    cache_rep->did_win_last_arbitration = false;
                    cache_rep->waited = 0; /* for debugging only - erase later */
                    entry->cache_rep = cache_rep;
                    if (dir_home == m_id) {
                        m_to_directory_rep_schedule_q.push_back(cache_rep);
                    } else {
                        shared_ptr<message_t> new_msg(new message_t);
                        new_msg->src = m_id;
                        new_msg->dst = dir_home;
                        new_msg->type = MSG_CACHE_REP;
                        new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                        new_msg->content = cache_rep;
                        entry->net_msg_to_send = new_msg;
                        m_to_network_schedule_q[MSG_CACHE_REP].push_back(new_msg);
                    }
                    entry->status = _L1_WORK_SEND_CACHE_REP;
                    ++it_addr;
                    continue;
                }
            } else {
                /* entry for a core request */
                if (l1_req->status() == CACHE_REQ_HIT) {
                    if (stats_enabled()) {
                        if (core_req->is_read()) {
                            stats()->did_read_l1(true);
                            stats()->did_finish_read(system_time - entry->requested_time);
                        } else {
                            stats()->did_write_l1(true);
                            stats()->did_finish_write(system_time - entry->requested_time);
                        }
                    }
                    shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                    uint32_t word_offset = (core_req->maddr().address / 4 )  % m_cfg.words_per_cache_line;
                    for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                        ret[i] = line->data[i + word_offset];
                    }
                    set_req_data(core_req, ret);
                    set_req_status(core_req, REQ_DONE);
                    ++m_l1_work_table_vacancy;
                    ++m_available_core_ports;
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a HIT and finish serving address " 
                        << core_req->maddr() << endl;
                    m_l1_work_table.erase(it_addr++);
                    continue;
                } else {
                    /* miss */
                    if (line && line->valid) {
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a coherence miss on " << start_maddr 
                                  << endl;
                        mh_assert(line_info->status == SHARED && !core_req->is_read());
                        l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_INVALIDATE));
                        l1_req->set_reserve(true); /* reserve the line after invalidation so it could get a exclusive copy */

                        /* cost breakdown study */
                        l1_req->set_milestone_time(system_time);

                        entry->l1_req = l1_req;
                        entry->status = _L1_WORK_INVALIDATE_SHARED_LINE;
                    } else if (line) {
                        if (victim) {
                            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] evicted a line for " << victim->start_maddr
                                      << " to make a space for " << start_maddr << endl;
                            uint32_t data_size = 0;
                            uint32_t dir_home = victim_info->directory_home;
                            cache_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                            cache_rep->sender = m_id;
                            cache_rep->receiver = dir_home;
                            cache_rep->waited = 0; /* for debugging only - erase later */

                            /* cost breakdown study */
                            cache_rep->milestone_time = system_time;

                            if (victim_info->status == SHARED) {
                                cache_rep->type = INV_REP;
                                if (stats_enabled()) {
                                    stats()->add_invrep();
                                }
                            } else {
                                cache_rep->type = FLUSH_REP;
                                if (stats_enabled()) {
                                    stats()->add_flushrep();
                                }
                                if (!m_cfg.use_mesi || victim->dirty) {
                                    data_size = m_cfg.words_per_cache_line * 4;
                                }
                            }
                            cache_rep->maddr = victim->start_maddr;
                            cache_rep->data = (data_size > 0) ? victim->data : shared_array<uint32_t>();
                            cache_rep->did_win_last_arbitration = false;
                            entry->cache_rep = cache_rep;
                            if (dir_home == m_id) {
                                m_to_directory_rep_schedule_q.push_back(cache_rep);
                            } else {
                                shared_ptr<message_t> new_msg(new message_t);
                                new_msg->src = m_id;
                                new_msg->dst = dir_home;
                                new_msg->type = MSG_CACHE_REP;
                                new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                                new_msg->content = cache_rep;
                                entry->net_msg_to_send = new_msg;
                                m_to_network_schedule_q[MSG_CACHE_REP].push_back(new_msg);
                            }
                            entry->status = _L1_WORK_SEND_CACHE_REP;
                        } else {
                            if (cat_req->status() == CAT_REQ_DONE) {
                                uint32_t dir_home = cat_req->home();
                                if (stats_enabled()) {
                                    stats()->did_read_cat(dir_home ==  m_id);
                                }
                                cache_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                                cache_req->sender = m_id;
                                cache_req->receiver = dir_home;
                                cache_req->type = core_req->is_read() ? SH_REQ : EX_REQ;
                                if (stats_enabled()) {
                                    if (core_req->is_read()) {
                                        stats()->add_shreq();
                                    } else {
                                        stats()->add_exreq();
                                    }
                                }
                                cache_req->maddr = start_maddr;
                                cache_req->data = shared_array<uint32_t>();
                                cache_req->did_win_last_arbitration = false;
                                cache_req->waited = 0; /* for debugging only - erase later */
                                /* only for debugging - erase later */
                                cache_req->waited = 0;

                                /* cost breakdown study */
                                cache_req->milestone_time = system_time;

                                entry->cache_req = cache_req;
                                if (dir_home == m_id) {
                                    m_to_directory_req_schedule_q.push_back(cache_req);
                                } else {
                                    shared_ptr<message_t> new_msg(new message_t);
                                    new_msg->src = m_id;
                                    new_msg->dst = dir_home;
                                    new_msg->type = MSG_CACHE_REQ;
                                    new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                                    new_msg->content = cache_req;
                                    entry->net_msg_to_send = new_msg;
                                    m_to_network_schedule_q[MSG_CACHE_REQ].push_back(new_msg);
                                }
                                entry->status = _L1_WORK_SEND_CACHE_REQ;
                            } else {
                                entry->status = _L1_WORK_READ_CAT;

                                /* cost breakdown study */
                                cat_req->set_milestone_time(system_time);

                            }
                        }
                    } else {
                        /* has to wait until some lines get directory replies and become evictable. just retry */
                        l1_req->reset();

                        /* cost breakdown study */
                        if (l1_req->milestone_time() != UINT64_MAX) {
                            l1_req->set_milestone_time(system_time);
                        }

                    }
                    ++it_addr;
                    continue;
                }
            }
        } else if (entry->status == _L1_WORK_INVALIDATE_SHARED_LINE) {
            if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            if (stats_enabled()) {
                mh_assert(l1_req->milestone_time() != UINT64_MAX);
                stats()->add_l1_action_cost(system_time - l1_req->milestone_time());
            }
            
            mh_assert(l1_req->status() == CACHE_REQ_HIT);
            uint32_t dir_home = line_info->directory_home;
            cache_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
            cache_rep->sender = m_id;
            cache_rep->receiver = dir_home;
            cache_rep->type = INV_REP;
            cache_rep->maddr = start_maddr;
            cache_rep->data = shared_array<uint32_t>();
            cache_rep->did_win_last_arbitration = false;
            cache_rep->waited = 0; /* for debugging only - erase later */

            if (stats_enabled()) {
                stats()->add_invrep();
            }

            /* cost breakdown study */
            cache_rep->milestone_time = system_time;
            
            entry->cache_rep = cache_rep;
            if (dir_home == m_id) {
                m_to_directory_rep_schedule_q.push_back(cache_rep);
            } else {
                shared_ptr<message_t> new_msg(new message_t);
                new_msg->src = m_id;
                new_msg->dst = dir_home;
                new_msg->type = MSG_CACHE_REP;
                new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                new_msg->content = cache_rep;
                entry->net_msg_to_send = new_msg;
                m_to_network_schedule_q[MSG_CACHE_REP].push_back(new_msg);
            }
            entry->status = _L1_WORK_SEND_CACHE_REP;
            ++it_addr;
            continue;
        } else if (entry->status == _L1_WORK_SEND_CACHE_REP) {
            if (cache_rep->did_win_last_arbitration) {

                /* cost breakdown study */
                if (cache_rep->milestone_time != UINT64_MAX) {
                    if (stats_enabled()) {
                        stats()->add_l1_eviction_cost(system_time - cache_rep->milestone_time);
                    }
                }

                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                cache_rep->did_win_last_arbitration = false;
                if (dir_req) {
                    /* this entry is due to a directory request. sending a reply finishes the job. */
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] has sent a cache reply to directory "
                        << cache_rep->receiver << " for address " << cache_rep->maddr << " upon a request." << endl;
                    ++m_l1_work_table_vacancy;
                    m_l1_work_table.erase(it_addr++);
                    continue;
                } else {
                    /* either a victim was evicted, or a current SHARED line is invalidated. */
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] has sent a cache reply to directory "
                        << cache_rep->receiver << " for address " << cache_rep->maddr
                        << " (evicted)." << endl;
                    if (cat_req->status() == CAT_REQ_DONE || (line && line->valid)) {
                        uint32_t dir_home;
                        if (cat_req->status() == CAT_REQ_DONE) {
                            dir_home = cat_req->home();
                        } else {
                            dir_home = line_info->directory_home;
                        }
                        if (!(line && line->valid) && stats_enabled()) {
                            stats()->did_read_cat(dir_home == m_id);
                        }
                        cache_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                        cache_req->sender = m_id;
                        cache_req->receiver = cat_req->home();
                        cache_req->type = core_req->is_read() ? SH_REQ : EX_REQ;
                        if (stats_enabled()) {
                            if (core_req->is_read()) {
                                stats()->add_shreq();
                            } else {
                                stats()->add_exreq();
                            }
                        }
                        cache_req->maddr = start_maddr;
                        cache_req->data = shared_array<uint32_t>();
                        cache_req->did_win_last_arbitration = false;
                        cache_req->waited = 0; /* for debugging only - erase later */

                        /* cost breakdown study */
                        cache_req->milestone_time = system_time;;

                        entry->cache_req = cache_req;
                        if (dir_home == m_id) {
                            m_to_directory_req_schedule_q.push_back(cache_req);
                        } else {
                            shared_ptr<message_t> new_msg(new message_t);
                            new_msg->src = m_id;
                            new_msg->dst = dir_home;
                            new_msg->type = MSG_CACHE_REQ;
                            new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                            new_msg->content = cache_req;
                            entry->net_msg_to_send = new_msg;
                            m_to_network_schedule_q[MSG_CACHE_REQ].push_back(new_msg);
                        }
                        entry->status = _L1_WORK_SEND_CACHE_REQ;
                    } else {
                        entry->status = _L1_WORK_READ_CAT;

                        /* cost breakdown study */
                        cat_req->set_milestone_time(system_time);

                    }
                    ++it_addr;
                    continue;
                }
            } else {
                if (cache_rep->receiver == m_id) {
                    m_to_directory_rep_schedule_q.push_back(cache_rep);
                } else {
                    m_to_network_schedule_q[MSG_CACHE_REP].push_back(entry->net_msg_to_send);
                }
                ++it_addr;
                continue;
            }
        } else if (entry->status == _L1_WORK_READ_CAT) {
            if (cat_req->status() == CAT_REQ_DONE) {

                /* cost breakdown study */
                if (cat_req->milestone_time() != UINT64_MAX) {
                    if (stats_enabled()) {
                        stats()->add_cat_action_cost(system_time - cat_req->milestone_time());
                    }
                }

                uint32_t dir_home = cat_req->home();
                if (stats_enabled()) {
                    stats()->did_read_cat(dir_home == m_id);
                }
                cache_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                cache_req->sender = m_id;
                cache_req->receiver = dir_home;
                cache_req->type = core_req->is_read() ? SH_REQ : EX_REQ;
                if (stats_enabled()) {
                    if (core_req->is_read()) {
                        stats()->add_shreq();
                    } else {
                        stats()->add_exreq();
                    }
                }
                cache_req->maddr = start_maddr;
                cache_req->data = shared_array<uint32_t>();
                cache_req->did_win_last_arbitration = false;
                cache_req->waited = 0; /* for debugging only - erase later */
                cache_req->milestone_time = system_time;
                entry->cache_req = cache_req;
                if (dir_home == m_id) {
                    m_to_directory_req_schedule_q.push_back(cache_req);
                } else {
                    shared_ptr<message_t> new_msg(new message_t);
                    new_msg->src = m_id;
                    new_msg->dst = dir_home;
                    new_msg->type = MSG_CACHE_REQ;
                    new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                    new_msg->content = cache_req;
                    entry->net_msg_to_send = new_msg;
                    m_to_network_schedule_q[MSG_CACHE_REQ].push_back(new_msg);
                }
                entry->status = _L1_WORK_SEND_CACHE_REQ;
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L1_WORK_SEND_CACHE_REQ) {
            if (cache_req->did_win_last_arbitration) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - cache_req->milestone_time);
                }

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent a cache request (" << cache_req->type 
                    << ") for " << cache_req->maddr << " to directory " << cache_req->receiver << endl;
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                cache_req->did_win_last_arbitration = false;
                entry->status = _L1_WORK_WAIT_DIRECTORY_REP;
            } else {
                if (cache_req->receiver == m_id) {
                    m_to_directory_req_schedule_q.push_back(cache_req);
                } else {
                    m_to_network_schedule_q[MSG_CACHE_REQ].push_back(entry->net_msg_to_send);
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L1_WORK_WAIT_DIRECTORY_REP) {
            if (dir_rep) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - dir_rep->milestone_time);
                }

                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a directory reply ("
                    << dir_rep->type << ") for " << start_maddr << endl;
                line_info = shared_ptr<cacheCoherenceInfo>(new cacheCoherenceInfo);
                line_info->directory_home = dir_rep->sender;
                line_info->status = dir_rep->type == SH_REP? SHARED : MODIFIED;
                shared_array<uint32_t> data = dir_rep->data;
                if (!core_req->is_read()) {
                    mh_assert(dir_rep->type == EX_REP);
                    uint32_t word_offset = (core_req->maddr().address / 4) % m_cfg.words_per_cache_line;
                    for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                        data[i + word_offset] = core_req->data()[i];
                    }
                }
                l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line, dir_rep->data, line_info));
                l1_req->set_clean_write(true);

                /* cost breakdown study */
                l1_req->set_milestone_time(system_time);

                entry->l1_req = l1_req;
                entry->status = _L1_WORK_FEED_L1_AND_FINISH;
            } 
            ++it_addr;
            continue;
        } else if (entry->status == _L1_WORK_FEED_L1_AND_FINISH) {
            if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            mh_assert(l1_req->milestone_time() != UINT64_MAX);
            if (stats_enabled()) {
                stats()->add_l1_action_cost(system_time - l1_req->milestone_time());
            }

            mh_assert(l1_req->status() == CACHE_REQ_HIT);
            if (stats_enabled()) {
                if (core_req->is_read()) {
                    stats()->did_read_l1(false);
                    stats()->did_finish_read(system_time - entry->requested_time);
                } else {
                    stats()->did_write_l1(false);
                    stats()->did_finish_write(system_time - entry->requested_time);
                }
            }
            shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
            uint32_t word_offset = (core_req->maddr().address / 4 )  % m_cfg.words_per_cache_line;
            for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                ret[i] = line->data[i + word_offset];
            }

            set_req_data(core_req, ret);
            set_req_status(core_req, REQ_DONE);
            ++m_l1_work_table_vacancy;
            ++m_available_core_ports;
            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] resolved a miss and finish serving address " 
                << core_req->maddr() << endl;
            m_l1_work_table.erase(it_addr++);
            continue;
        }
    }
}

void privateSharedMSI::process_cache_rep(shared_ptr<cacheLine> line, shared_ptr<coherenceMsg> rep) {

    uint32_t sender = rep->sender;
    shared_ptr<directoryCoherenceInfo> info = 
        static_pointer_cast<directoryCoherenceInfo>(line->coherence_info);

    mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] received a cache rep from sender : " << sender 
        << " for " << rep->maddr << " (" << rep->type << ") current: " ;

    if (info->directory.empty()) {
        mh_log(4) << " (empty)";
    }
    for (set<uint32_t>::iterator it = info->directory.begin(); it != info->directory.end(); ++it) {
        mh_log(4) << " " << *it << " ";
    }
    mh_log(4) << endl;

    if (rep->type == WB_REP) {
        mh_assert(info->directory.size() == 1);
        info->status = READERS;
    } else {
        /* invRep or flushRep */
        info->directory.erase(sender);
        if (info->directory.empty()) {
            info->status = READERS;
        }
    }
    if (rep->data) {
        /* wbRep or flushRep */
        line->dirty = true;
        line->data = rep->data;
    }

}

void privateSharedMSI::l2_work_table_update() {
    for (toL2Table::iterator it_addr = m_l2_work_table.begin(); it_addr != m_l2_work_table.end(); ) {
        maddr_t start_maddr = it_addr->first;
        shared_ptr<toL2Entry> entry = it_addr->second;
        shared_ptr<cacheRequest> l2_req = entry->l2_req;
        shared_ptr<coherenceMsg> cache_req = entry->cache_req;
        shared_ptr<coherenceMsg> cache_rep = entry->cache_rep;
        shared_ptr<coherenceMsg> dir_rep = entry->dir_rep;
        shared_ptr<dramMsg> dram_req = entry->dram_req;
        shared_ptr<dramMsg> dram_rep = entry->dram_rep;
        shared_ptr<cacheLine> line = l2_req->line_copy();
        shared_ptr<directoryCoherenceInfo> line_info = 
            line ? static_pointer_cast<directoryCoherenceInfo>(line->coherence_info) : shared_ptr<directoryCoherenceInfo>();;
        shared_ptr<cacheLine> victim = l2_req->victim_line_copy();
        shared_ptr<directoryCoherenceInfo> victim_info = 
            victim ? static_pointer_cast<directoryCoherenceInfo>(victim->coherence_info) : shared_ptr<directoryCoherenceInfo>();;

        if (entry->status == _L2_WORK_READ_L2) {
            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            if (l2_req->milestone_time() != UINT64_MAX) {
                if (stats_enabled()) {
                    stats()->add_l2_action_cost(system_time - l2_req->milestone_time());
                }
            }

            if (!cache_req) {
                mh_assert(cache_rep);
                mh_assert(line_info->directory.count(cache_rep->sender));
                mh_assert(l2_req->status() == CACHE_REQ_HIT); /* must have not evicted because the directory is not empty */
                process_cache_rep(line, cache_rep);
                l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE, 
                                                                   line->dirty? m_cfg.words_per_cache_line : 0,
                                                                   line->data,
                                                                   line_info));

                /* cost breakdown study */
                l2_req->set_milestone_time(UINT64_MAX);

                l2_req->set_clean_write(!line->dirty);
                entry->l2_req = l2_req;
                entry->status = _L2_WORK_UPDATE_L2_AND_FINISH;
                ++it_addr;
                continue;
            } else if (cache_req->type == EMPTY_REQ) {
                mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " received an empty request for " << start_maddr << endl;
                if (l2_req->status() == CACHE_REQ_MISS) {
                    /* before this request is fired, others already invalidated this line */
                    mh_assert(!cache_rep); /* there must be no cache replies that has not been processed */
                    if (entry->using_space_for_evict) {
                        ++m_l2_work_table_vacancy_evict;
                    } else if (entry->using_space_for_reply) {
                        ++m_l2_work_table_vacancy_replies;
                    } else {
                        ++m_l2_work_table_vacancy_shared;
                    }
                    m_l2_work_table.erase(it_addr++);
                    continue;
                } else {
                    /* hit */
                    if (line_info->directory.empty()) {
                        l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_INVALIDATE));
                        l2_req->set_reserve(false);
                        entry->l2_req = l2_req;
                        entry->status = _L2_WORK_UPDATE_L2_AND_FINISH;

                        /* cost breakdown study */
                        l2_req->set_milestone_time(UINT64_MAX);

                    } else {
                        for (set<uint32_t>::iterator it = line_info->directory.begin(); it != line_info->directory.end(); ++it) {
                            shared_ptr<coherenceMsg> dir_req(new coherenceMsg);
                            dir_req->sender = m_id;
                            dir_req->receiver = *it;
                            dir_req->type = line_info->status == READERS ? INV_REQ : FLUSH_REQ;
                            if (stats_enabled()) {
                                if (line_info->status == READERS) {
                                    stats()->add_invreq();
                                } else {
                                    stats()->add_flushreq();
                                }
                            }
                            dir_req->maddr = start_maddr;
                            dir_req->data = shared_array<uint32_t>();
                            dir_req->did_win_last_arbitration = false;
                            dir_req->waited = 0; /* for debugging only. erase later */
                            entry->dir_reqs.push_back(dir_req);
                            line_info->status = WAITING_FOR_REPLIES;
                        }
                        entry->invalidate_begin_time = system_time;
                        entry->invalidate_num_targets = entry->dir_reqs.size();
                        entry->accept_cache_replies = true;
                        entry->status = _L2_WORK_EMPTY_LINE_TO_EVICT;
                    }
                    ++it_addr;
                    continue;
                }
            } else {
                /* requests from L1s */
                if (l2_req->status() == CACHE_REQ_HIT) {
                    uint32_t sender = cache_req->sender;
                    if (line_info->directory.count(sender)) {
                        /* have to wait for a reply first */
                        entry->status = _L2_WORK_REORDER_CACHE_REP;

                        entry->accept_cache_replies = true;

                        mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " received a request for " << start_maddr
                                  << " while " << sender << " is still in the directory. waiting for the reply" << endl;

                        /* cost breakdown study */
                        entry->milestone_time = system_time;

                        ++it_addr;
                        continue;
                    }
                    if (line_info->status == READERS) {
                        if (cache_req->type == SH_REQ || line_info->directory.empty()) {

                            if (stats_enabled()) {
                                if (line_info->directory.empty()) {
                                    if (cache_req->type == SH_REQ) {
                                        if (m_cfg.use_mesi) {
                                            stats()->add_i_e();
                                        } else {
                                            stats()->add_i_s();
                                        }
                                    } else {
                                        stats()->add_i_e();
                                    }
                                } else {
                                    stats()->add_s_s();
                                }
                            }

                            /* TODO : add the number of sharers constraints */
                            if (cache_req->type == EX_REQ ||
                                (m_cfg.use_mesi && line_info->directory.empty()) ) 
                            {
                                line_info->status = WRITER;
                            }
                            line_info->directory.insert(sender);
                            l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                               m_cfg.words_per_cache_line, 
                                                                               line->data, 
                                                                               line_info));
                            l2_req->set_clean_write(line->dirty);
                            l2_req->set_reserve(true);

                            /* cost breakdown study */
                            l2_req->set_milestone_time(system_time);

                            entry->l2_req = l2_req;
                            entry->status = _L2_WORK_UPDATE_L2_AND_SEND_REP;
                        } else {

                            if (stats_enabled()) {
                                stats()->add_s_e();
                            }
                            for (set<uint32_t>::iterator it = line_info->directory.begin(); 
                                 it != line_info->directory.end(); ++it)
                            {
                                shared_ptr<coherenceMsg> dir_req(new coherenceMsg);
                                dir_req->sender = m_id;
                                dir_req->receiver = *it;
                                dir_req->type = INV_REQ;
                                if (stats_enabled()) {
                                    stats()->add_invreq();
                                }
                                dir_req->maddr = start_maddr;
                                dir_req->data = shared_array<uint32_t>();
                                dir_req->did_win_last_arbitration = false;
                                dir_req->waited = 0; /* for debugging only. erase later */
                                entry->dir_reqs.push_back(dir_req);
                                line_info->status = WAITING_FOR_REPLIES;
                            }
                            entry->accept_cache_replies = true;
                            entry->status = _L2_WORK_INVALIDATE_CACHES;
                            entry->invalidate_begin_time = system_time;
                            entry->invalidate_num_targets = entry->dir_reqs.size();

                            /* cost breakdown study */
                            entry->milestone_time = system_time;

                        }
                    } else {
                        mh_assert(line_info->status == WRITER);
                        shared_ptr<coherenceMsg> dir_req(new coherenceMsg);
                        dir_req->sender = m_id;
                        dir_req->receiver = *line_info->directory.begin();
                        dir_req->type = cache_req->type == SH_REQ? WB_REQ : FLUSH_REQ;
                        if (stats_enabled()) {
                            if (cache_req->type == SH_REQ) {
                                stats()->add_wbreq();
                                stats()->add_e_s();
                            } else {
                                stats()->add_flushreq();
                                stats()->add_e_e();
                            }
                        }
                        dir_req->maddr = start_maddr;
                        dir_req->data = shared_array<uint32_t>();
                        dir_req->did_win_last_arbitration = false;
                        dir_req->waited = 0; /* for debugging only. erase later */
                        entry->dir_reqs.push_back(dir_req);
                        line_info->status = WAITING_FOR_REPLIES;
                        entry->accept_cache_replies = true;
                        entry->status = _L2_WORK_INVALIDATE_CACHES;
                        entry->invalidate_begin_time = system_time;
                        entry->invalidate_num_targets = entry->dir_reqs.size();

                        /* cost breakdown study */
                        entry->milestone_time = system_time;

                    }
                    ++it_addr;
                    continue;
                } else {
                    /* miss */
                    entry->did_miss_on_first = true;
                    if (line) {

                        if (stats_enabled()) {
                            if (cache_req->type == SH_REQ && !m_cfg.use_mesi) {
                                stats()->add_i_s();
                            } else {
                                stats()->add_i_e();
                            }
                        }

                        /* cost breakdown study */

                        if (victim) {
                            mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] evicted a line for " 
                                      << victim->start_maddr << " to make a space for " << start_maddr << endl;
                        }

                        if (victim && victim->dirty) {
                            dram_req = shared_ptr<dramMsg>(new dramMsg);
                            dram_req->sender = m_id;
                            dram_req->receiver = m_dramctrl_location;
                            dram_req->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                    DRAM_REQ_WRITE,
                                                                                    m_cfg.words_per_cache_line,
                                                                                    victim->data));
                            dram_req->did_win_last_arbitration = false;

                            /* cost breakdown study */
                            dram_req->milestone_time = system_time;

                            entry->dram_req = dram_req;
                            if (m_dramctrl_location == m_id) {
                                m_to_dram_req_schedule_q.push_back(entry->dram_req);
                            } else {
                                entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                                entry->net_msg_to_send->src = m_id;
                                entry->net_msg_to_send->dst = m_dramctrl_location;
                                entry->net_msg_to_send->type = MSG_DRAM_REQ;
                                entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                                entry->net_msg_to_send->content = dram_req;
                                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                            }
                            entry->status = _L2_WORK_DRAM_WRITEBACK_AND_REQUEST;
                        } else {
                            dram_req = shared_ptr<dramMsg>(new dramMsg);
                            dram_req->sender = m_id;
                            dram_req->receiver = m_dramctrl_location;
                            dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                    DRAM_REQ_READ,
                                                                                    m_cfg.words_per_cache_line));
                            dram_req->did_win_last_arbitration = false;
                            entry->dram_req = dram_req;

                            /* cost breakdown study */
                            dram_req->milestone_time = system_time;

                            if (m_dramctrl_location == m_id) {
                                m_to_dram_req_schedule_q.push_back(entry->dram_req);
                            } else {
                                entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                                entry->net_msg_to_send->src = m_id;
                                entry->net_msg_to_send->dst = m_dramctrl_location;
                                entry->net_msg_to_send->type = MSG_DRAM_REQ;
                                entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes);
                                entry->net_msg_to_send->content = dram_req;
                                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                            }
                            entry->status = _L2_WORK_SEND_DRAM_FEED_REQ;
                        }
                    } else {
                        if (victim && m_l2_work_table.count(victim->start_maddr) == 0) {
                            mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] tries to invalidate caches of line for " 
                                      << start_maddr << " to evict the line " << endl;
                            shared_ptr<coherenceMsg> new_msg(new coherenceMsg);
                            new_msg->sender = m_id;
                            new_msg->receiver = m_id;
                            new_msg->type = EMPTY_REQ;
                            new_msg->maddr = victim->start_maddr;
                            new_msg->data = shared_array<uint32_t>();
                            new_msg->did_win_last_arbitration = false;
                            new_msg->waited = 0; /* debugging only. erase later */
                            m_to_directory_req_schedule_q.push_back(new_msg);
                        }

                        l2_req->set_milestone_time(system_time);
                        
                        /* will keep trying until a line is successfully emptied. */
                        l2_req->reset();
                    }
                    ++it_addr;
                    continue;
                }
            }
        } else if (entry->status == _L2_WORK_UPDATE_L2_AND_FINISH) {
            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            if (l2_req->milestone_time() != UINT64_MAX) {
                if (stats_enabled()) {
                    stats()->add_l2_action_cost(system_time - l2_req->milestone_time());
                }
            }

            mh_assert(l2_req->status() == CACHE_REQ_HIT);
            if (entry->using_space_for_evict) {
                ++m_l2_work_table_vacancy_evict;
            } else if (entry->using_space_for_reply) {
                ++m_l2_work_table_vacancy_replies;
            } else {
                ++m_l2_work_table_vacancy_shared;
            }
            m_l2_work_table.erase(it_addr++);
            continue;
        } else if (entry->status == _L2_WORK_DRAM_WRITEBACK_AND_EVICT) {
            if (dram_req->did_win_last_arbitration) {
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                dram_req->did_win_last_arbitration = false;
                mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] sent a DRAM writeback for "
                          << dram_req->req->maddr() << endl;
                entry->status = _L2_WORK_UPDATE_L2_AND_FINISH;
            } else if (m_dramctrl_location == m_id) {
                m_to_dram_req_schedule_q.push_back(entry->dram_req);
            } else {
                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_EMPTY_LINE_TO_EVICT) {
            if (cache_rep) {
                process_cache_rep(line, cache_rep);
                entry->cache_rep = shared_ptr<coherenceMsg>();
                if (line_info->directory.empty()) {
                    entry->accept_cache_replies = false;
                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_INVALIDATE));
                    l2_req->set_reserve(false);

                    /* cost breakdown study */
                    l2_req->set_milestone_time(UINT64_MAX);

                    entry->l2_req = l2_req;
                    if (stats_enabled()) {
                        stats()->did_invalidate_caches(entry->invalidate_num_targets, system_time - entry->invalidate_begin_time);
                    }
                    if (line->dirty) {
                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dramctrl_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(line->start_maddr,
                                                                                DRAM_REQ_WRITE,
                                                                                m_cfg.words_per_cache_line,
                                                                                line->data));
                        dram_req->did_win_last_arbitration = false;

                        /* cost breakdown study */
                        dram_req->milestone_time = UINT64_MAX;

                        entry->dram_req = dram_req;
                        if (m_dramctrl_location == m_id) {
                            m_to_dram_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dramctrl_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                            entry->net_msg_to_send->content = dram_req;
                            m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }

                        entry->status = _L2_WORK_DRAM_WRITEBACK_AND_EVICT;
                    } else {
                        entry->status = _L2_WORK_UPDATE_L2_AND_FINISH;
                    }
                    ++it_addr;
                    continue;
                }
            }
            /* this is supposed to happen in the previous cycle */
            while (entry->dir_reqs.size()) {
                shared_ptr<coherenceMsg> dir_req = entry->dir_reqs.front();
                if (dir_req->did_win_last_arbitration) {
                    mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] has sent a directory request "
                        << "to " << dir_req->receiver << " for " << dir_req->maddr << endl;
                    if (entry->net_msg_to_send) {
                        entry->net_msg_to_send = shared_ptr<message_t>();
                    }
                    dir_req->did_win_last_arbitration = false;
                    entry->dir_reqs.erase(entry->dir_reqs.begin());
                } else {
                    break;
                }
            }
            if (entry->dir_reqs.size()) {
                shared_ptr<coherenceMsg> dir_req = entry->dir_reqs.front();
                if (dir_req->receiver == m_id) {
                    m_to_cache_req_schedule_q.push_back(make_tuple(false/* not a core request */, dir_req));
                } else {
                    if (!entry->net_msg_to_send) {
                        shared_ptr<message_t> new_msg(new message_t);
                        new_msg->type = MSG_DIRECTORY_REQ_REP;
                        new_msg->src = m_id;
                        new_msg->dst = dir_req->receiver;
                        new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                        new_msg->content = dir_req;
                        entry->net_msg_to_send = new_msg;
                    }
                    m_to_network_schedule_q[MSG_DIRECTORY_REQ_REP].push_back(entry->net_msg_to_send);
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_REORDER_CACHE_REP) {
            if (cache_rep) {
                process_cache_rep(line, cache_rep);
                entry->cache_rep = shared_ptr<coherenceMsg>();
                if (line_info->directory.count(cache_req->sender) == 0) {

                    /* cost breakdown study */
                    if (stats_enabled()) {
                        stats()->add_l2_network_plus_serialization_cost(system_time - entry->milestone_time);
                    }

                    entry->accept_cache_replies = false;
                    entry->status = _L2_WORK_READ_L2;
                    l2_req->set_milestone_time(UINT64_MAX);
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_INVALIDATE_CACHES) {
            if (cache_rep) {
                process_cache_rep(line, cache_rep);
                entry->cache_rep = shared_ptr<coherenceMsg>();
                if ((cache_req->type == SH_REQ && line_info->status == READERS) ||
                    line_info->directory.empty()) {

                    /* cost breakdown study */
                    if (stats_enabled()) {
                        stats()->add_l2_invalidation_cost(system_time - entry->milestone_time);
                    }

                    uint32_t sender = cache_req->sender;
                    entry->accept_cache_replies = false;
                    if (cache_req->type == EX_REQ || (m_cfg.use_mesi && line_info->directory.empty())) {
                        line_info->status = WRITER;
                    }
                    line_info->directory.insert(sender);
                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                       m_cfg.words_per_cache_line,
                                                                       line->data,
                                                                       line_info));
                    l2_req->set_clean_write(line->dirty);
                    l2_req->set_reserve(true);

                    /* cost breakdown study */
                    l2_req->set_milestone_time(system_time);

                    entry->l2_req = l2_req;
                    entry->status = _L2_WORK_UPDATE_L2_AND_SEND_REP;
                    if (stats_enabled()) {
                        stats()->did_invalidate_caches(entry->invalidate_num_targets, system_time - entry->invalidate_begin_time);
                    }
                    ++it_addr;
                    continue;
                }
            }
            /* this is supposed to happen in the previous cycle */
            while (entry->dir_reqs.size()) {
                shared_ptr<coherenceMsg> dir_req = entry->dir_reqs.front();
                if (dir_req->did_win_last_arbitration) {
                    mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] has sent a directory request "
                        << "to " << dir_req->receiver << " for " << dir_req->maddr << endl;
                    if (entry->net_msg_to_send) {
                        entry->net_msg_to_send = shared_ptr<message_t>();
                    }
                    dir_req->did_win_last_arbitration = false;
                    entry->dir_reqs.erase(entry->dir_reqs.begin());
                } else if (line_info->directory.count(dir_req->receiver) == 0) {
                    mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] already received a cache reply "
                        << " for " << dir_req->maddr << " so do not send a request " << endl;
                    if (entry->net_msg_to_send) {
                        entry->net_msg_to_send = shared_ptr<message_t>();
                    }
                    dir_req->did_win_last_arbitration = false;
                    entry->dir_reqs.erase(entry->dir_reqs.begin());

                } else {
#if 1
                    if (++dir_req->waited > 10000) {
                        mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] cannot send a directory request "
                                  << "to " << dir_req->receiver << " for " << dir_req->maddr;
                        mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] cannot send a directory request "
                                  << "to " << dir_req->receiver << " for " << dir_req->maddr;
                    }
                    if (dir_req->waited > 10100) {
                        throw err_bad_shmem_cfg("seems like a deadlock");
                    }
#endif

                    break;
                }
            }
            if (entry->dir_reqs.size()) {
                shared_ptr<coherenceMsg> dir_req = entry->dir_reqs.front();
                if (dir_req->receiver == m_id) {
                    m_to_cache_req_schedule_q.push_back(make_tuple(false/* not a core request */, dir_req));
                } else {
                    if (!entry->net_msg_to_send) {
                        shared_ptr<message_t> new_msg(new message_t);
                        new_msg->type = MSG_DIRECTORY_REQ_REP;
                        new_msg->src = m_id;
                        new_msg->dst = dir_req->receiver;
                        new_msg->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                        new_msg->content = dir_req;
                        entry->net_msg_to_send = new_msg;
                    }
                    m_to_network_schedule_q[MSG_DIRECTORY_REQ_REP].push_back(entry->net_msg_to_send);
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_UPDATE_L2_AND_SEND_REP) {
            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            if (l2_req->milestone_time() != UINT64_MAX) {
                if (stats_enabled()) {
                    stats()->add_l2_action_cost(system_time - l2_req->milestone_time());
                }
            }

            if (l2_req->status() == CACHE_REQ_HIT) {
                uint32_t sender = cache_req->sender;
                dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                dir_rep->sender = m_id;
                dir_rep->receiver = sender;
                dir_rep->type = line_info->status == READERS ? SH_REP : EX_REP;
                if (stats_enabled()) {
                    if (line_info->status == READERS) {
                        stats()->add_shrep();
                    } else {
                        stats()->add_exrep();
                    }
                }
                dir_rep->maddr = start_maddr;
                dir_rep->data = line->data;
                dir_rep->did_win_last_arbitration = false;
                dir_rep->waited = 0; /* debugging only. erase later */

                /* cost breakdown study */
                dir_rep->milestone_time = system_time;

                entry->dir_rep = dir_rep;
                if (sender == m_id) {
                    mh_assert(m_l1_work_table.count(start_maddr) &&
                              m_l1_work_table[start_maddr]->status == _L1_WORK_WAIT_DIRECTORY_REP);
                    m_l1_work_table[start_maddr]->dir_rep = entry->dir_rep;
                    if (stats_enabled()) {
                        stats()->did_access_l2(!entry->did_miss_on_first);
                    }
                    mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] sent a directory reply to "
                        << m_id << " for " << start_maddr << endl;
                    if (entry->using_space_for_evict) {
                        ++m_l2_work_table_vacancy_evict;
                    } else if (entry->using_space_for_reply) {
                        ++m_l2_work_table_vacancy_replies;
                    } else {
                        ++m_l2_work_table_vacancy_shared;
                    }
                    m_l2_work_table.erase(it_addr++);
                    continue;
                } else {
                    entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                    entry->net_msg_to_send->type = MSG_DIRECTORY_REQ_REP;
                    entry->net_msg_to_send->src = m_id;
                    entry->net_msg_to_send->dst = sender;
                    entry->net_msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                    entry->net_msg_to_send->content = dir_rep;
                    m_to_network_schedule_q[MSG_DIRECTORY_REQ_REP].push_back(entry->net_msg_to_send);
                    entry->status = _L2_WORK_SEND_DIRECTORY_REP;
                    ++it_addr;
                    continue;
                }
            } else {
                /* miss */
                if (line) {
                    if (victim && victim->dirty) {
                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dramctrl_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                DRAM_REQ_WRITE,
                                                                                m_cfg.words_per_cache_line,
                                                                                victim->data));
                        dram_req->did_win_last_arbitration = false;

                        /* cost breakdown study */
                        dram_req->milestone_time = system_time;

                        entry->dram_req = dram_req;
                        if (m_dramctrl_location == m_id) {
                            m_to_dram_req_schedule_q.push_back(entry->dram_req);
                        } else {
                            entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                            entry->net_msg_to_send->src = m_id;
                            entry->net_msg_to_send->dst = m_dramctrl_location;
                            entry->net_msg_to_send->type = MSG_DRAM_REQ;
                            entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes + m_cfg.words_per_cache_line * 4);
                            entry->net_msg_to_send->content = dram_req;
                            m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                        }
                        entry->status = _L2_WORK_DRAM_WRITEBACK_AND_UPDATE;
                    } else {

                        /* cost breakdown study */
                        l2_req->set_milestone_time(system_time);

                        l2_req->reset(); /* retry */
                    }
                } else {
                    if (victim && m_l2_work_table.count(victim->start_maddr) == 0) {
                        shared_ptr<coherenceMsg> new_msg(new coherenceMsg);
                        new_msg->sender = m_id;
                        new_msg->receiver = m_id;
                        new_msg->type = EMPTY_REQ;
                        new_msg->maddr = victim->start_maddr;
                        new_msg->data = shared_array<uint32_t>();
                        new_msg->did_win_last_arbitration = false;
                        new_msg->waited = 0; /* debugging only. erase later */
                        m_to_directory_req_schedule_q.push_back(new_msg);
                    }

                    /* cost breadown study */
                    l2_req->set_milestone_time(system_time);

                    l2_req->reset();
                }
                ++it_addr;
                continue;
            }
        } else if (entry->status == _L2_WORK_DRAM_WRITEBACK_AND_UPDATE) {
            if (dram_req->did_win_last_arbitration) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_network_plus_serialization_cost(system_time - dram_req->milestone_time);
                }
                l2_req->set_milestone_time(system_time);

                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                dram_req->did_win_last_arbitration = false;
                mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] sent a DRAM writeback for "
                          << dram_req->req->maddr() << endl;
                l2_req->reset();
                entry->status = _L2_WORK_UPDATE_L2_AND_SEND_REP;
            } else if (m_dramctrl_location == m_id) {
                m_to_dram_req_schedule_q.push_back(entry->dram_req);
            } else {
                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_DRAM_WRITEBACK_AND_REQUEST) {
            if (dram_req->did_win_last_arbitration) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_network_plus_serialization_cost(system_time - dram_req->milestone_time);
                }

                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                dram_req->did_win_last_arbitration = false;

                mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] sent a DRAM writeback for "
                          << dram_req->req->maddr() << endl;

                dram_req = shared_ptr<dramMsg>(new dramMsg);
                dram_req->sender = m_id;
                dram_req->receiver = m_dramctrl_location;
                dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                        DRAM_REQ_READ,
                                                                        m_cfg.words_per_cache_line));
                dram_req->did_win_last_arbitration = false;

                /* cost breakdown study */
                dram_req->milestone_time = system_time;

                entry->dram_req = dram_req;
                if (m_dramctrl_location == m_id) {
                    m_to_dram_req_schedule_q.push_back(entry->dram_req);
                } else {
                    entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                    entry->net_msg_to_send->src = m_id;
                    entry->net_msg_to_send->dst = m_dramctrl_location;
                    entry->net_msg_to_send->type = MSG_DRAM_REQ;
                    entry->net_msg_to_send->flit_count = get_flit_count (1 + m_cfg.address_size_in_bytes);
                    entry->net_msg_to_send->content = dram_req;
                    m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                }
                entry->status = _L2_WORK_SEND_DRAM_FEED_REQ;
            } else if (m_dramctrl_location == m_id) {
                m_to_dram_req_schedule_q.push_back(entry->dram_req);
            } else {
                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_SEND_DRAM_FEED_REQ) {
            if (dram_req->did_win_last_arbitration) {

                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                dram_req->did_win_last_arbitration = false;

                mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] sent a DRAM request for "
                          << dram_req->req->maddr() << endl;

                entry->status = _L2_WORK_WAIT_DRAM_FEED;
            } else if (m_dramctrl_location == m_id) {
                m_to_dram_req_schedule_q.push_back(entry->dram_req);
            } else {
                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_WAIT_DRAM_FEED) {
            if (dram_rep) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_network_plus_serialization_cost(system_time - dram_rep->milestone_time);
                }

                mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] received a DRAM reply for "
                          << dram_rep->req->maddr() << endl;
                line_info = shared_ptr<directoryCoherenceInfo>(new directoryCoherenceInfo);
                if (cache_req->type == EX_REQ || m_cfg.use_mesi) {
                    line_info->status = WRITER;
                } else {
                    line_info->status = READERS;
                }
                line_info->directory.insert(cache_req->sender);
                l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr,
                                                                   CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line,
                                                                   dram_rep->req->read(), line_info));
                l2_req->set_clean_write(true);

                /* cost breakdown study */
                l2_req->set_milestone_time(system_time);

                entry->l2_req = l2_req;
                entry->status = _L2_WORK_UPDATE_L2_AND_SEND_REP;
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_SEND_DIRECTORY_REP) {
            if (dir_rep->did_win_last_arbitration) {
                dir_rep->did_win_last_arbitration = false;
                if (stats_enabled()) {
                    stats()->did_access_l2(!entry->did_miss_on_first);
                }
                mh_log(4) << "[DIRECTORY " << m_id << " @ " << system_time << " ] sent a directory reply to "
                    << dir_rep->receiver << " for " << start_maddr << endl;
                if (entry->using_space_for_evict) {
                    ++m_l2_work_table_vacancy_evict;
                } else if (entry->using_space_for_reply) {
                    ++m_l2_work_table_vacancy_replies;
                } else {
                    ++m_l2_work_table_vacancy_shared;
                }
                m_l2_work_table.erase(it_addr++);
                continue;
            } else {
                m_to_network_schedule_q[MSG_DIRECTORY_REQ_REP].push_back(entry->net_msg_to_send);
                ++it_addr;
                continue;
            }
        }
    }

}

void privateSharedMSI::dram_work_table_update() {
    for (toDRAMTable::iterator it_addr = m_dram_work_table.begin(); it_addr != m_dram_work_table.end(); ) {
        shared_ptr<toDRAMEntry> entry = it_addr->second;
        if (entry->dram_req->req->status() == DRAM_REQ_DONE) {

            /* cost breakdown study */
            if (stats_enabled()) {
                stats()->add_dram_offchip_network_plus_dram_action_cost(system_time - entry->milestone_time);
            }

            if (!entry->dram_rep) {
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
                mh_assert(m_l2_work_table.count(start_maddr) && m_l2_work_table[start_maddr]->status == _L2_WORK_WAIT_DRAM_FEED);
                m_l2_work_table[start_maddr]->dram_rep = entry->dram_rep;
                mh_log(4) << "[DRAM " << m_id << " @ " << system_time << " ] has sent a dram rep for address " << entry->dram_rep->req->maddr()
                          << " to core " << m_id << endl;
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
                    mh_log(4) << "[DRAM " << m_id << " @ " << system_time << " ] has sent a dram rep for address " 
                              << entry->dram_rep->req->maddr() << " to core " << entry->dram_req->sender << endl;
                    m_dram_work_table.erase(it_addr++);
                    continue;
                }
            }
        }
        ++it_addr;
    }
}

void privateSharedMSI::accept_incoming_messages() {

    while (m_core_receive_queues[MSG_DIRECTORY_REQ_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DIRECTORY_REQ_REP]->front();
        shared_ptr<coherenceMsg> cc_msg = static_pointer_cast<coherenceMsg>(msg->content);
        if (cc_msg->did_win_last_arbitration) {
            /* reset the flag for the next arbitration */
            cc_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_DIRECTORY_REQ_REP]->pop();
            /* this pop is supposed to be done in the previous cycle. continue to the next cycle */
           continue;
        } else {

            /* for debugging only - erase later */
#if 1
            if (++cc_msg->waited > 10000) {
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cannot receive a directory ";
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cannot receive a directory ";
                if (cc_msg->type == SH_REP || cc_msg->type == EX_REP) {
                    mh_log(4) << "reply (";
                    mh_log(4) << "reply (";
                } else {
                    mh_log(4) << "request (";
                    mh_log(4) << "request (";
                } 
                mh_log(4) << cc_msg->type 
                    << ") for " << cc_msg->maddr  
                    << " from " << msg->src << " cannot get in"  
                    << " (table:size " << m_l1_work_table.size() << " ) ";
                if (m_l1_work_table.count(cc_msg->maddr)) {
                    mh_log(4) << "existing entry state : " << m_l1_work_table[cc_msg->maddr]->status << endl;
                    mh_log(4) << "existing entry state : " << m_l1_work_table[cc_msg->maddr]->status << endl;
                } else {
                    mh_log(4) << "table full" << endl;
                    mh_log(4) << "table full" << endl;
                }
            }
            if (cc_msg->waited > 10100) {
                throw err_bad_shmem_cfg("seems like a deadlock");
            }
#endif

            if (cc_msg->type == SH_REP || cc_msg->type == EX_REP) {
                /* guaranteed to accept */
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] received a directory reply for " 
                          << cc_msg->maddr << endl;
                mh_assert(m_l1_work_table.count(cc_msg->maddr) && 
                          m_l1_work_table[cc_msg->maddr]->status == _L1_WORK_WAIT_DIRECTORY_REP &&
                          !m_l1_work_table[cc_msg->maddr]->dir_rep);
                m_l1_work_table[cc_msg->maddr]->dir_rep = cc_msg;
                m_core_receive_queues[MSG_DIRECTORY_REQ_REP]->pop();
            } else {
                /* directory -> cache requests */
                m_to_cache_req_schedule_q.push_back(make_tuple(false, cc_msg));
            }
            break;
        }
    }

    while (m_core_receive_queues[MSG_CACHE_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_REQ]->front();
        shared_ptr<coherenceMsg> cc_msg = static_pointer_cast<coherenceMsg>(msg->content);
        if (cc_msg->did_win_last_arbitration) {
            /* reset the flag for the next arbitration */
            cc_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_CACHE_REQ]->pop();
            /* this pop is supposed to be done in the previous cycle. continue to the next cycle */
            continue;
        } else {

            /* erase later */
#if 1
            if (++cc_msg->waited > 10000) {
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cannot receive a cache req (" << cc_msg->type 
                    << ") for " << cc_msg->maddr  
                    << " from " << msg->src << " cannot get in"  
                    << " (table:size " << m_l2_work_table.size() << " ) ";
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cannot receive a cache req (" << cc_msg->type 
                    << ") for " << cc_msg->maddr  
                    << " from " << msg->src << " cannot get in"  
                    << " (table:size " << m_l2_work_table.size() << " ) ";
                if (m_l2_work_table.count(cc_msg->maddr)) {
                    mh_log(4) << "existing entry state : " << m_l2_work_table[cc_msg->maddr]->status << endl;
                    mh_log(4) << "existing entry state : " << m_l2_work_table[cc_msg->maddr]->status << endl;
                } else {
                    mh_log(4) << "table full" << endl;
                    mh_log(4) << "table full" << endl;
                }
            }
            if (cc_msg->waited > 10100) {
                throw err_bad_shmem_cfg("seems like a deadlock");
            }
#endif

            m_to_directory_req_schedule_q.push_back(cc_msg);
            break;
        }
    }

    while (m_core_receive_queues[MSG_CACHE_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_REP]->front();
        shared_ptr<coherenceMsg> cc_msg = static_pointer_cast<coherenceMsg>(msg->content);
        if (cc_msg->did_win_last_arbitration) {
            /* reset the flag for the next arbitration */
            cc_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_CACHE_REP]->pop();
            /* this pop is supposed to be done in the previous cycle. continue to the next cycle */
            continue;
        } else {

            /* erase later */
#if 1
            if (++cc_msg->waited > 10000) {
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cannot receive a cache rep (" << cc_msg->type 
                    << ") for " << cc_msg->maddr  
                    << " from " << msg->src << " cannot get in"  
                    << " (table:size " << m_l2_work_table.size() << " ) ";
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] cannot receive a cache rep (" << cc_msg->type 
                    << ") for " << cc_msg->maddr  
                    << " from " << msg->src << " cannot get in"  
                    << " (table:size " << m_l2_work_table.size() << " ) ";
                if (m_l2_work_table.count(cc_msg->maddr)) {
                    mh_log(4) << "existing entry state : " << m_l2_work_table[cc_msg->maddr]->status << endl;
                    mh_log(4) << "existing entry state : " << m_l2_work_table[cc_msg->maddr]->status << endl;
                } else {
                    mh_log(4) << "table full" << endl;
                    mh_log(4) << "table full" << endl;
                }
            }
            if (cc_msg->waited > 10100) {
                throw err_bad_shmem_cfg("seems like a deadlock");
            }
#endif

            m_to_directory_rep_schedule_q.push_back(cc_msg);
            break;
        }
    }

    while (m_core_receive_queues[MSG_DRAM_REQ]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAM_REQ]->front();
        shared_ptr<dramMsg> dram_msg = static_pointer_cast<dramMsg>(msg->content);
        if (dram_msg->did_win_last_arbitration) {
            dram_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_DRAM_REQ]->pop();
            continue;
        } else {
            m_to_dram_req_schedule_q.push_back(dram_msg);
            break;
        }
    }

    if (m_core_receive_queues[MSG_DRAM_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAM_REP]->front();
        shared_ptr<dramMsg> dram_msg = static_pointer_cast<dramMsg>(msg->content);
        maddr_t start_maddr = dram_msg->req->maddr(); /* always access by a cache line */
        /* always for a read */
        mh_assert(m_l2_work_table.count(start_maddr) > 0 && m_l2_work_table[start_maddr]->status == _L2_WORK_WAIT_DRAM_FEED);
        m_l2_work_table[start_maddr]->dram_rep = dram_msg;
        m_core_receive_queues[MSG_DRAM_REP]->pop();
    }

}

void privateSharedMSI::schedule_requests() {

    /* random arbitration */
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);


    /* 1 : arbitrates requests from the core. */
    /*     the core is assumed to have a finite number of access ports to the memory */
    /*     this ports are hold by accepted requests until it is eventually served    */
    random_shuffle(m_core_port_schedule_q.begin(), m_core_port_schedule_q.end(), rr_fn);
    uint32_t count = 0;
    while (m_core_port_schedule_q.size()) {
        shared_ptr<memoryRequest> req = m_core_port_schedule_q.front();
        if (count < m_available_core_ports) {
            m_to_cache_req_schedule_q.push_back(make_tuple(true, req));
            ++count;
        } else {
            set_req_status(req, REQ_RETRY);
        }
        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
    }
    
    /* 2 : arbitrates l1 work table for new entries */
    random_shuffle(m_to_cache_req_schedule_q.begin(), m_to_cache_req_schedule_q.end(), rr_fn);
    while (m_to_cache_req_schedule_q.size()) {
        bool is_core_req = m_to_cache_req_schedule_q.front().get<0>();
        if (is_core_req) {
            shared_ptr<memoryRequest> req = 
                static_pointer_cast<memoryRequest>(m_to_cache_req_schedule_q.front().get<1>());
            maddr_t start_maddr = get_start_maddr_in_line(req->maddr());
            if (m_l1_work_table.count(start_maddr) || m_l1_work_table_vacancy == 0 || m_available_core_ports == 0) {
                mh_log(4) << "you have to retry" << endl;
                set_req_status(req, REQ_RETRY);

                m_to_cache_req_schedule_q.erase(m_to_cache_req_schedule_q.begin());
                continue;
            }

            if (stats_enabled()) {
                stats()->add_memory_subsystem_serialization_cost(system_time - req->serialization_begin_time());
            }

            shared_ptr<toL1Entry> new_entry(new toL1Entry);
            new_entry->status = _L1_WORK_READ_L1;
            new_entry->core_req = req;

            shared_ptr<cacheRequest> l1_req
                (new cacheRequest(req->maddr(), 
                                  req->is_read()? CACHE_REQ_READ : CACHE_REQ_WRITE,
                                  req->word_count(),
                                  req->is_read()? shared_array<uint32_t>() : req->data()));

            /* cost breakdown study */
            l1_req->set_milestone_time(system_time);

            new_entry->l1_req = l1_req;
           
            shared_ptr<catRequest> cat_req(new catRequest(req->maddr(), m_id));

            /* cost breakdown study */
            cat_req->set_milestone_time(UINT64_MAX);

            new_entry->cat_req = cat_req;

            new_entry->dir_req = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->cache_rep = shared_ptr<coherenceMsg>();
            new_entry->requested_time = system_time;
            new_entry->net_msg_to_send = shared_ptr<message_t>();

            set_req_status(req, REQ_WAIT);
            --m_available_core_ports;

            m_l1_work_table[start_maddr] = new_entry;
            --m_l1_work_table_vacancy;

            m_to_cache_req_schedule_q.erase(m_to_cache_req_schedule_q.begin());

        } else {
            /* a request from directory */
            shared_ptr<coherenceMsg> msg = 
                static_pointer_cast<coherenceMsg>(m_to_cache_req_schedule_q.front().get<1>());
            maddr_t start_maddr = msg->maddr;
            if (m_l1_work_table.count(start_maddr)) {
                /* discard this directory request if currency core request gets or got a miss */
                if ((m_l1_work_table[start_maddr]->status != _L1_WORK_READ_L1) &&
                    !(m_l1_work_table[start_maddr]->dir_rep))
                {
                    msg->did_win_last_arbitration = true; /* will be discarded from the network queue */
                    mh_log(4) << "[L1 " << m_id  << " @ " << system_time << " ] discarded a directory request (" << msg->type 
                        << ") for address " << msg->maddr << " (state: " << m_l1_work_table[start_maddr]->status << " ) " << endl;
                }
            } else if (m_l1_work_table_vacancy) {
                shared_ptr<toL1Entry> new_entry(new toL1Entry);
                new_entry->status = _L1_WORK_READ_L1;
                new_entry->core_req = shared_ptr<memoryRequest>();

                mh_log(4) << "[L1 " << m_id  << " @ " << system_time << " ] received a directory request (" << msg->type 
                    << ") for address " << msg->maddr << endl;

                shared_ptr<cacheRequest> l1_req;
                if (msg->type == WB_REQ) {
                    shared_ptr<cacheCoherenceInfo> new_info(new cacheCoherenceInfo);
                    new_info->status = SHARED;
                    new_info->directory_home = msg->sender;
                    l1_req = shared_ptr<cacheRequest>(new cacheRequest(msg->maddr, CACHE_REQ_UPDATE,
                                                                       0, shared_array<uint32_t>(), new_info));
                } else {
                    /* invReq or flushReq */
                    l1_req = shared_ptr<cacheRequest>(new cacheRequest(msg->maddr, CACHE_REQ_INVALIDATE));
                }
                l1_req->set_reserve(false);

                /* cost breakdown study */
                /* directory request cost is all considered as invalidation cost */
                l1_req->set_milestone_time(UINT64_MAX); /* will not count */

                new_entry->l1_req = l1_req;

                new_entry->cat_req = shared_ptr<catRequest>();
                new_entry->dir_req = msg;
                new_entry->dir_rep = shared_ptr<coherenceMsg>();
                new_entry->cache_req = shared_ptr<coherenceMsg>();
                new_entry->cache_req = shared_ptr<coherenceMsg>();
                new_entry->net_msg_to_send = shared_ptr<message_t>();

                msg->did_win_last_arbitration = true;

                m_l1_work_table[start_maddr] = new_entry;
                --m_l1_work_table_vacancy;
            }
            m_to_cache_req_schedule_q.erase(m_to_cache_req_schedule_q.begin());
        }
    }
    m_to_cache_req_schedule_q.clear();

    random_shuffle(m_to_directory_rep_schedule_q.begin(), m_to_directory_rep_schedule_q.end(), rr_fn);
    while (m_to_directory_rep_schedule_q.size()) {
        shared_ptr<coherenceMsg> msg = m_to_directory_rep_schedule_q.front();
        maddr_t start_maddr = msg->maddr;
        if (m_l2_work_table.count(start_maddr)) {
            if (m_l2_work_table[start_maddr]->accept_cache_replies && !m_l2_work_table[start_maddr]->cache_rep) {
                mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache reply (" << msg->type 
                          << ") from " << msg->sender << " for address " << msg->maddr 
                          << " (state: " << m_l2_work_table[start_maddr]->status << ")" << endl;
                m_l2_work_table[start_maddr]->cache_rep = msg;
                msg->did_win_last_arbitration = true;
            }
        } else if (m_l2_work_table_vacancy_replies) {
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache reply from " << msg->sender 
                << " for address " << msg->maddr << " (new entry - reserved) " << endl;
            shared_ptr<toL2Entry> new_entry(new toL2Entry);
            new_entry->accept_cache_replies = false;
            new_entry->using_space_for_evict = false;
            new_entry->using_space_for_reply = true;
            new_entry->status = _L2_WORK_READ_L2;
            new_entry->did_miss_on_first = false;
            new_entry->cache_rep = msg;

            shared_ptr<cacheRequest> l2_req(new cacheRequest(msg->maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
            l2_req->set_reserve(true);
            new_entry->l2_req = l2_req;

            /* cost breakdown study */
            l2_req->set_milestone_time(UINT64_MAX);

            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->dram_req = shared_ptr<dramMsg>();
            new_entry->dram_rep = shared_ptr<dramMsg>();

            msg->did_win_last_arbitration = true;

            m_l2_work_table[start_maddr] = new_entry;
            --m_l2_work_table_vacancy_replies;

        } else if (m_l2_work_table_vacancy_shared) {
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache reply from " << msg->sender 
                      << " for address " << msg->maddr << " (new entry) " << endl;
            shared_ptr<toL2Entry> new_entry(new toL2Entry);
            new_entry->accept_cache_replies = false;
            new_entry->using_space_for_evict = false;
            new_entry->using_space_for_reply = false;
            new_entry->status = _L2_WORK_READ_L2;
            new_entry->did_miss_on_first = false;
            new_entry->cache_rep = msg;

            shared_ptr<cacheRequest> l2_req(new cacheRequest(msg->maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
            l2_req->set_reserve(true);
            new_entry->l2_req = l2_req;

            /* cost breakdown study */
            l2_req->set_milestone_time(UINT64_MAX);

            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->dram_req = shared_ptr<dramMsg>();
            new_entry->dram_rep = shared_ptr<dramMsg>();

            msg->did_win_last_arbitration = true;

            m_l2_work_table[start_maddr] = new_entry;
            --m_l2_work_table_vacancy_shared;
        }
        m_to_directory_rep_schedule_q.erase(m_to_directory_rep_schedule_q.begin());
    }
    random_shuffle(m_to_directory_req_schedule_q.begin(), m_to_directory_req_schedule_q.end(), rr_fn);
    while (m_to_directory_req_schedule_q.size()) {
        shared_ptr<coherenceMsg> msg = m_to_directory_req_schedule_q.front();
        maddr_t start_maddr = msg->maddr;
        if (m_l2_work_table.count(start_maddr)) {
            /* need to finish the previous request first */
            m_to_directory_req_schedule_q.erase(m_to_directory_req_schedule_q.begin());
            continue;
        }
        if (msg->type == EMPTY_REQ && m_l2_work_table_vacancy_evict) {
            /* the hardware needs a dedicated space for invalidating L1 caches in order to evict a line */
            /* otherwise, it may deadlock when all entries are trying to evict a line but there's no space in the table */
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache request from " << msg->sender 
                      << " for address " << msg->maddr << " (new entry) " << endl;
            shared_ptr<toL2Entry> new_entry(new toL2Entry);
            new_entry->accept_cache_replies = true;
            new_entry->using_space_for_evict = true;
            new_entry->using_space_for_reply = false;
            new_entry->status = _L2_WORK_READ_L2;
            new_entry->did_miss_on_first = false;
            new_entry->cache_req = msg;

            shared_ptr<cacheRequest> l2_req(new cacheRequest(msg->maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
            l2_req->set_reserve(false); /* if it misses, no need to bring the line in */
            new_entry->l2_req = l2_req;

            new_entry->cache_rep = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->dram_req = shared_ptr<dramMsg>();
            new_entry->dram_rep = shared_ptr<dramMsg>();

            msg->did_win_last_arbitration = true;

            /* cost breakdown study */
            l2_req->set_milestone_time(UINT64_MAX);

            m_l2_work_table[start_maddr] = new_entry;
            --m_l2_work_table_vacancy_evict;
        } else if (m_l2_work_table_vacancy_shared) {
            mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received a cache request from " << msg->sender 
                      << " for address " << msg->maddr << " (new entry) " << endl;
            shared_ptr<toL2Entry> new_entry(new toL2Entry);
            new_entry->accept_cache_replies = false;
            new_entry->using_space_for_evict = false;
            new_entry->using_space_for_reply = false;
            new_entry->status = _L2_WORK_READ_L2;
            new_entry->cache_req = msg;

            shared_ptr<cacheRequest> l2_req(new cacheRequest(msg->maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
            l2_req->set_reserve(true);
            new_entry->l2_req = l2_req;

            new_entry->cache_rep = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->dram_req = shared_ptr<dramMsg>();
            new_entry->dram_rep = shared_ptr<dramMsg>();

            msg->did_win_last_arbitration = true;

            /* cost breakdown study */
            l2_req->set_milestone_time(system_time);
            if (msg->type != EMPTY_REQ && stats_enabled()) {
                stats()->add_l2_network_plus_serialization_cost(system_time - msg->milestone_time);
            }

            m_l2_work_table[start_maddr] = new_entry;
            --m_l2_work_table_vacancy_shared;
        }
        m_to_directory_req_schedule_q.erase(m_to_directory_req_schedule_q.begin());
    }
    
    /* 4 : arbitrate inputs to dram work table */
    random_shuffle(m_to_dram_req_schedule_q.begin(), m_to_dram_req_schedule_q.end(), rr_fn);
    while (m_to_dram_req_schedule_q.size()) {
        mh_assert(m_dramctrl);
        shared_ptr<dramMsg> msg = m_to_dram_req_schedule_q.front();
        if (m_dramctrl->available()) {
            if (msg->req->is_read()) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_network_plus_serialization_cost(system_time - msg->milestone_time);
                }

                mh_assert(!m_dram_work_table.count(msg->req->maddr()));
                shared_ptr<toDRAMEntry> new_entry(new toDRAMEntry);
                new_entry->dram_req = msg;
                new_entry->dram_rep = shared_ptr<dramMsg>();
                new_entry->net_msg_to_send = shared_ptr<message_t>();

                /* cost breakdown study */
                new_entry->milestone_time = system_time;

                m_dram_work_table[msg->req->maddr()] = new_entry;
            }
                /* if write, make a request and done */
            m_dramctrl->request(msg->req);
            msg->did_win_last_arbitration = true;
        }
        m_to_dram_req_schedule_q.erase(m_to_dram_req_schedule_q.begin());
    }

    /* try make cache requests */
    for (toL1Table::iterator it_addr = m_l1_work_table.begin(); it_addr != m_l1_work_table.end(); ++it_addr) {
        maddr_t start_maddr = it_addr->first;
        shared_ptr<toL1Entry> entry = it_addr->second;
        if (entry->l1_req && entry->l1_req->status() == CACHE_REQ_NEW) {
            /* not requested yet */
            shared_ptr<cacheRequest> l1_req = entry->l1_req;
            if (l1_req->request_type() == CACHE_REQ_WRITE || 
                l1_req->request_type() == CACHE_REQ_UPDATE ||
                l1_req->request_type() == CACHE_REQ_WRITE) {
                m_l1_write_req_schedule_q.push_back(l1_req);
            } else {
                m_l1_read_req_schedule_q.push_back(l1_req);
            }
        }
        if (entry->cat_req && entry->cat_req->status() == CAT_REQ_NEW) {
                m_cat_req_schedule_q.push_back(entry->cat_req);
        }
    }

    for (toL2Table::iterator it_addr = m_l2_work_table.begin(); it_addr != m_l2_work_table.end(); ++it_addr) {
        maddr_t start_maddr = it_addr->first;
        shared_ptr<toL2Entry> entry = it_addr->second;
        if (entry->l2_req && entry->l2_req->status() == CACHE_REQ_NEW) {
            shared_ptr<cacheRequest> l2_req = entry->l2_req;
            if (l2_req->request_type() == CACHE_REQ_WRITE || 
                l2_req->request_type() == CACHE_REQ_UPDATE || 
                l2_req->request_type() == CACHE_REQ_WRITE) {
                m_l2_write_req_schedule_q.push_back(l2_req);
            } else {
                m_l2_read_req_schedule_q.push_back(l2_req);
            }
        }
    }

    /* cat requests */
    random_shuffle(m_cat_req_schedule_q.begin(), m_cat_req_schedule_q.end(), rr_fn);
    while (m_cat->available() && m_cat_req_schedule_q.size()) {
        m_cat->request(m_cat_req_schedule_q.front());

        /* cost breakdown study */
        if (m_cat_req_schedule_q.front()->milestone_time() != UINT64_MAX) {
            if (stats_enabled()) {
                stats()->add_cat_serialization_cost(system_time - m_cat_req_schedule_q.front()->milestone_time());
            }
            m_cat_req_schedule_q.front()->set_milestone_time(system_time);
        }

        m_cat_req_schedule_q.erase(m_cat_req_schedule_q.begin());
    }
    m_cat_req_schedule_q.clear();

    /* l1 read requests */
    random_shuffle(m_l1_read_req_schedule_q.begin(), m_l1_read_req_schedule_q.end(), rr_fn);
    while (m_l1->read_port_available() && m_l1_read_req_schedule_q.size()) {
        m_l1->request(m_l1_read_req_schedule_q.front());

        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        if (m_l1_read_req_schedule_q.front()->milestone_time() != UINT64_MAX) {
            if (stats_enabled()) {
                stats()->add_l1_serialization_cost(system_time - m_l1_read_req_schedule_q.front()->milestone_time());
            }
            m_l1_read_req_schedule_q.front()->set_milestone_time(system_time);
        }

        m_l1_read_req_schedule_q.erase(m_l1_read_req_schedule_q.begin());
    }
    m_l1_read_req_schedule_q.clear();
    
    /* l1 write requests */
    random_shuffle(m_l1_write_req_schedule_q.begin(), m_l1_write_req_schedule_q.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q.size()) {
        m_l1->request(m_l1_write_req_schedule_q.front());

        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_l1_action();
        }
        if (m_l1_write_req_schedule_q.front()->milestone_time() != UINT64_MAX) {
            if (stats_enabled()) {
                stats()->add_l1_serialization_cost(system_time - m_l1_write_req_schedule_q.front()->milestone_time());
            }
            m_l1_write_req_schedule_q.front()->set_milestone_time(system_time);
        }

        m_l1_write_req_schedule_q.erase(m_l1_write_req_schedule_q.begin());
    }
    m_l1_write_req_schedule_q.clear();

    /* l2 read requests */
    set<maddr_t> issued_start_maddrs; 
    random_shuffle(m_l2_read_req_schedule_q.begin(), m_l2_read_req_schedule_q.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q.size()) {
        shared_ptr<cacheRequest> req = m_l2_read_req_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(req->maddr());
        if (issued_start_maddrs.count(start_maddr) == 0) {
            m_l2->request(req);
            issued_start_maddrs.insert(start_maddr);

            /* cost breakdown study */
            if (stats_enabled()) {
                stats()->add_l2_action();
            }
            if (req->milestone_time() != UINT64_MAX) {
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - req->milestone_time());
                }
                req->set_milestone_time(system_time);
            }

        }
        m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
    }
    issued_start_maddrs.clear();
    m_l2_read_req_schedule_q.clear();

    /* l2 write requests */
    random_shuffle(m_l2_write_req_schedule_q.begin(), m_l2_write_req_schedule_q.end(), rr_fn);
    while (m_l2->write_port_available() && m_l2_write_req_schedule_q.size()) {
        shared_ptr<cacheRequest> req = m_l2_write_req_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(req->maddr());
        m_l2->request(req);
        
        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_l2_action();
        }
        if (req->milestone_time() != UINT64_MAX) {
            if (stats_enabled()) {
                stats()->add_l2_network_plus_serialization_cost(system_time - req->milestone_time());
            }
            req->set_milestone_time(system_time);
        }

        m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
    }
    m_l2_write_req_schedule_q.clear();

    /* networks */
    for (uint32_t it_channel = 0; it_channel < NUM_MSG_TYPES; ++it_channel) {
        random_shuffle(m_to_network_schedule_q[it_channel].begin(), m_to_network_schedule_q[it_channel].end(), rr_fn);
        while (m_to_network_schedule_q[it_channel].size()) {
            shared_ptr<message_t> msg = m_to_network_schedule_q[it_channel].front();
            if (m_core_send_queues[it_channel]->push_back(msg)) {
                mh_log(4) << "[NET " << m_id << " @ " << system_time << " ] network msg gone " << m_id << " -> " << msg->dst << " type " << it_channel << " num flits " << msg->flit_count << endl;
                switch (it_channel) {
                case MSG_CACHE_REQ:
                case MSG_CACHE_REP:
                case MSG_DIRECTORY_REQ_REP:
                    {
                        shared_ptr<coherenceMsg> cc_msg = static_pointer_cast<coherenceMsg>(msg->content);
                        cc_msg->did_win_last_arbitration = true;
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


