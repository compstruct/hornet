// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSI.hpp"
#include "messages.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>

/* 64-bit address */
#define ADDRESS_SIZE 8

#define DEBUG

#ifdef DEBUG
#define mh_log cerr
#define mh_assert(X) assert(X)
#else
#define mh_assert(X)
#define mh_log LOG(log,4)
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

static void directory_will_return_line(cacheLine &line) { 
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<privateSharedMSI::directoryCoherenceInfo>(new privateSharedMSI::directoryCoherenceInfo);
    }
    static_pointer_cast<privateSharedMSI::directoryCoherenceInfo>(line.coherence_info)->hold = true;
}

static bool cache_is_hit(shared_ptr<cacheRequest> req, cacheLine& line) { 
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(line.coherence_info);
    switch (req->request_type()) {
    case CACHE_REQ_COHERENCE_READ:
        return info->status == privateSharedMSI::SHARED || info->status == privateSharedMSI::MODIFIED;
    case CACHE_REQ_COHERENCE_WRITE:
        return info->status == privateSharedMSI::MODIFIED;
    case CACHE_REQ_COHERENCE_INVALIDATE:
        return info->status != privateSharedMSI::PENDING;
    default:
        return false;
    }
}

static bool directory_is_hit(shared_ptr<cacheRequest> req, cacheLine &line) { 
    shared_ptr<privateSharedMSI::directoryCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::directoryCoherenceInfo>(line.coherence_info);
    return !info->hold;
}

static void cache_reserve_empty_line(cacheLine &line) { 
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<privateSharedMSI::cacheCoherenceInfo>(new privateSharedMSI::cacheCoherenceInfo);
    }
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(line.coherence_info);
    info->status = privateSharedMSI::PENDING;
    return; 
}

static void directory_reserve_empty_line(cacheLine &line) { 
    /* just create info (if none) and let the cache know it need to reserve an empty line for a miss */
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<privateSharedMSI::directoryCoherenceInfo>(new privateSharedMSI::directoryCoherenceInfo);
    }
    return; 
}

static bool cache_can_evict_line(cacheLine &line) {
    shared_ptr<privateSharedMSI::cacheCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::cacheCoherenceInfo>(line.coherence_info);
    return info->status != privateSharedMSI::PENDING;
}

static bool directory_can_evict_line(cacheLine &line) { 
    shared_ptr<privateSharedMSI::directoryCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::directoryCoherenceInfo>(line.coherence_info);
    return !info->hold && info->directory.empty();
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
    m_l2_work_table_vacancy(cfg.l2_work_table_size),
    m_available_core_ports(cfg.num_local_core_ports)
{
    mh_assert(m_cfg.bytes_per_flit > 0);
    mh_assert(m_cfg.words_per_cache_line > 0);
    mh_assert(m_cfg.lines_in_l1 > 0);
    mh_assert(m_cfg.lines_in_l2 > 0);

    m_l1 = new cache(id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l1, cfg.l1_associativity, cfg.l1_replacement_policy, 
                     cfg.l1_hit_test_latency, cfg.l1_num_read_ports, cfg.l1_num_write_ports);
    m_l2 = new cache(id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l2, cfg.l2_associativity, cfg.l2_replacement_policy, 
                     cfg.l2_hit_test_latency, cfg.l2_num_read_ports, cfg.l2_num_write_ports);

    m_l1->set_helper_copy_coherence_info(&cache_copy_coherence_info);
    m_l1->set_helper_is_hit(&cache_is_hit);
    m_l1->set_helper_reserve_empty_line(&cache_reserve_empty_line);
    m_l1->set_helper_can_evict_line(&cache_can_evict_line);

    m_l2->set_helper_copy_coherence_info(&directory_copy_coherence_info);
    m_l2->set_helper_will_return_line(&directory_will_return_line);
    m_l2->set_helper_is_hit(&directory_is_hit);
    m_l2->set_helper_reserve_empty_line(&directory_reserve_empty_line);
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
    schedule_requests();

    m_l1->tick_positive_edge();
    m_l2->tick_positive_edge();
    m_cat->tick_positive_edge();
    if(m_dram_controller) {
        m_dram_controller->tick_positive_edge();
    }
}

void privateSharedMSI::tick_negative_edge() {

    m_l1->tick_negative_edge();
    m_l2->tick_negative_edge();
    m_cat->tick_negative_edge();
    if(m_dram_controller) {
        m_dram_controller->tick_negative_edge();
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
        if (entry->status == _L1_WORK_WAIT_L1) {
            mh_assert(!entry->dir_rep);
            shared_ptr<cacheRequest> l1_req = entry->l1_req;
            if (l1_req->status() == CACHE_REQ_HIT) {
                if (entry->core_req) {
                    /* was a core request */
                    if (stats_enabled()) {
                        if (entry->core_req->is_read()) {   
                            stats()->did_read_l1(true); /* only stats for core requests not dir req (always hit) */
                            stats()->did_finish_read(system_time - entry->requested_time);
                        } else {
                            stats()->did_write_l1(true);
                            stats()->did_finish_write(system_time - entry->requested_time);
                        }
                    }
                    set_req_data(entry->core_req, entry->l1_req->line_copy()->data);
                    set_req_status(entry->core_req, REQ_DONE);
                    /* _L1_WORK_END */
                    m_l1_work_table.erase(it_addr++);
                    ++m_l1_work_table_vacancy;
                    ++m_available_core_ports;
                    continue;
                } else {
                    /* was a directory request */
                    shared_ptr<coherenceMsg> dir_req = entry->dir_req;
                    shared_ptr<cacheLine> line = l1_req->line_copy();
                    if (dir_req->type == INV_REQ) {
                        if (line) {
                            mh_assert(!line->dirty);
                            shared_ptr<coherenceMsg> rep(new coherenceMsg);
                            rep->sender = m_id;
                            rep->type = INV_REP;
                            rep->maddr = start_maddr;
                            rep->data = shared_array<uint32_t>();
                            rep->did_win_last_arbitration = false;
                            entry->cache_rep = rep;
                            entry->status = _L1_WORK_SEND_REP;
                        } else {
                            /* the line was already invalidated. discard the request */
                            /* _L1_WORK_END */
                            m_l1_work_table.erase(it_addr++);
                            ++m_l1_work_table_vacancy;
                            continue;
                        }
                    } else {
                        /* flushReq or wbReq */
                        mh_assert(line);
                        shared_ptr<coherenceMsg> rep(new coherenceMsg);
                        rep->sender = m_id;
                        rep->type = dir_req->type == FLUSH_REQ ? FLUSH_REP : WB_REP;
                        rep->maddr = start_maddr;
                        rep->data = line->data;
                        rep->did_win_last_arbitration = false;
                        entry->cache_rep = rep;
                        entry->status = _L1_WORK_SEND_REP;
                    }
                }
            } else if (l1_req->status() == CACHE_REQ_MISS) {
                shared_ptr<cacheLine> victim = l1_req->victim_line_copy();
                if (!victim) {
                    /* an empty line is rerserved (pending stats) */
                    entry->status = _L1_WORK_WAIT_CAT;
                } else {
                    mh_assert(victim->coherence_info);
                    shared_ptr<cacheCoherenceInfo> info = static_pointer_cast<cacheCoherenceInfo>(victim->coherence_info);
                    if (cache_can_evict_line(*victim)) {
                        shared_ptr<coherenceMsg> rep(new coherenceMsg);
                        rep->sender = m_id;
                        rep->type = info->status == MODIFIED ? FLUSH_REP : WB_REP;
                        rep->maddr = start_maddr;
                        rep->data = victim->data;
                        rep->did_win_last_arbitration = false;
                        entry->cache_rep = rep;
                        entry->status = _L1_WORK_SEND_REP;
                    } else {
                        /* couldn't invalidate - retry */
                        entry->l1_req->reset(); /* the scheduler will try to make an L1 request again in the next cycle */
                        entry->status = _L1_WORK_WAIT_L1;
                    }
                }
                mh_log << "[MEM " << m_id << " @ " << system_time << " ] req from core is missed on L1 for start address "
                     << hex << start_maddr<< dec << endl;
            }
            it_addr++;
            continue;
        } else if (entry->status == _L1_WORK_WAIT_REP) {
            if (entry->dir_rep) {
                mh_log << "[L1 " << m_id << " @ " << system_time << " ] received a directory reply for address " 
                     << start_maddr << endl;
                shared_ptr<cacheCoherenceInfo> info(new cacheCoherenceInfo);
                info->directory_home = entry->cat_req->home();
                info->status = entry->dir_rep->type == SH_REP ? SHARED : MODIFIED;
                shared_ptr<cacheRequest> l1_req(new cacheRequest(start_maddr, CACHE_REQ_UPDATE, m_cfg.words_per_cache_line,
                                                                 entry->dir_rep->data, info));
                entry->l1_req = l1_req;
                entry->status = _L1_WORK_FEED_L1;
            }
            it_addr++;
            continue;
        } else if (entry->status == _L1_WORK_FEED_L1) {
            mh_assert(entry->core_req);
            mh_assert(entry->l1_req->status() != CACHE_REQ_MISS);
            if (entry->l1_req->status() == CACHE_REQ_HIT) {
                mh_log << "[MEM " << m_id << " @ " << system_time << " ] finished feeding L1 for address " << start_maddr << endl;
                if (stats_enabled()) {
                    mh_assert(entry->core_req);
                    if (entry->core_req->is_read()) {
                        stats()->did_read_l1(false);
                        stats()->did_finish_read(system_time - entry->requested_time);
                    } else {
                        stats()->did_write_l1(false);
                        stats()->did_finish_write(system_time - entry->requested_time);
                    }
                }
                set_req_status(entry->core_req, REQ_DONE);
                set_req_data(entry->core_req, entry->dir_rep->data);
                /* _L1_WORK_END */
                m_l1_work_table.erase(it_addr++);
                ++m_l1_work_table_vacancy;
                ++m_available_core_ports;
                continue;
            }
            it_addr++;
            continue;
        }
        if (entry->status == _L1_WORK_WAIT_CAT) {
            /* note: do not else this if - it includes the case from _L1_WORK_WAIT_L1 at the same cycle */
            mh_assert(entry->cat_req);
            mh_assert(entry->core_req);
            if (entry->cat_req->status() != CAT_REQ_DONE) {
                ++it_addr;
                continue;
            } else {
                shared_ptr<coherenceMsg> req(new coherenceMsg);
                req->sender = m_id;
                req->receiver = entry->cat_req->home();
                req->type = entry->core_req->is_read() ? SH_REQ : EX_REQ;
                req->maddr = start_maddr;
                req->data = shared_array<uint32_t>();
                req->did_win_last_arbitration = false;
                entry->cache_req = req;
                entry->status = _L1_WORK_SEND_REQ;
            }
        }
        if (entry->status == _L1_WORK_SEND_REQ) {
            /* note: do not else this if - it includes the case from _L1_WORK_WAIT_L1 at the same cycle */
            uint32_t dir_home = entry->cat_req->home();
            if (entry->cache_req->did_win_last_arbitration) {
                /* reset the flag for the next arbitration */
                /* this is supposed to be done in the previous cycle, but doing here is equivalent */
                entry->cache_req->did_win_last_arbitration = false;
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                mh_log << "[L1 " << m_id << " @ " << system_time << " ] has sent a cache req to directory at " 
                     << dir_home << " for address " << hex << start_maddr<< dec << endl;
                entry->status = _L1_WORK_WAIT_REP;
            } else if (dir_home != m_id) {
                if (!entry->net_msg_to_send) {
                    shared_ptr<message_t> msg(new message_t);
                    msg->type = MSG_CACHE_REQ;
                    msg->src = m_id;
                    msg->dst = dir_home;
                    msg->flit_count = (1 + ADDRESS_SIZE + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;
                    msg->content = entry->cache_req;
                    entry->net_msg_to_send = msg;
                }
                m_to_network_schedule_q[MSG_CACHE_REQ].push_back(entry->net_msg_to_send);
            } else {
                m_to_directory_req_schedule_q.push_back(entry->cache_req);
            }
        } 
        if (entry->status == _L1_WORK_SEND_REP) {
            /* note: do not else this if - it includes the case from _L1_WORK_WAIT_L1 at the same cycle */
            mh_assert(entry->l1_req);
            shared_ptr<cacheLine> line = entry->l1_req->line_copy();
            mh_assert(line);
            uint32_t dir_home = static_pointer_cast<cacheCoherenceInfo>(line->coherence_info)->directory_home;
            if (entry->cache_rep->did_win_last_arbitration) {
                /* reset the flag for the next arbitration */
                /* this is supposed to be done in the previous cycle, but doing here is equivalent */
                entry->cache_rep->did_win_last_arbitration = false;
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                if (entry->core_req) {
                    entry->status = _L1_WORK_WAIT_CAT;
                } else {
                    mh_assert(entry->dir_req);
                    mh_log << "[L1 " << m_id << " @ " << system_time << " ] sends back a cc reply for start address "
                               << hex << entry->cache_rep->maddr<< dec << endl;
                    m_l1_work_table.erase(it_addr++);
                    ++m_l1_work_table_vacancy;
                    continue;
                }
            } else if (dir_home != m_id) {
                if (!entry->net_msg_to_send) {
                    shared_ptr<message_t> msg(new message_t);
                    msg->type = MSG_CACHE_REP;
                    msg->src = m_id;
                    msg->dst = dir_home;
                    uint32_t bytes = 1 + ADDRESS_SIZE; /* 1 for message */
                    if (entry->cache_rep->type != INV_REP) {
                        bytes += m_cfg.words_per_cache_line * 4;
                    }
                    msg->flit_count = (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;;
                    msg->content = entry->cache_rep;
                    entry->net_msg_to_send = msg;
                }
                m_to_network_schedule_q[MSG_CACHE_REP].push_back(entry->net_msg_to_send);
            } else {
                /* here on and on. source is correct. at the next cycle received a wrong source */
                m_to_directory_rep_schedule_q.push_back(entry->cache_rep);
            }
        }
        ++it_addr;
    }
}

void privateSharedMSI::process_cache_rep(shared_ptr<cacheLine> line, shared_ptr<coherenceMsg> rep) {

    uint32_t sender = rep->sender;
    shared_ptr<directoryCoherenceInfo> info = 
        static_pointer_cast<directoryCoherenceInfo>(line->coherence_info);
#if 1
    mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] received a cache rep from sender : " << sender << endl;
    for (set<uint32_t>::iterator it = info->directory.begin(); it != info->directory.end(); ++it) {
        if (sender != *it) {
            mh_log << " " << *it << " ";
        }
    }
    mh_log << endl;
#endif

    if (info->directory.count(sender) == 0) {
        return;
    }
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
    if (rep->type != INV_REP) {
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
        
        if (entry->status == _L2_WORK_UPDATE_L2_AND_RESTART) {
            if (stats_enabled()) {
                stats()->did_write_l2(true);
            }
            mh_assert(l2_req->status() != CACHE_REQ_MISS);
            if (l2_req->status() == CACHE_REQ_HIT) {
                entry->l2_req = shared_ptr<cacheRequest>(new cacheRequest(cache_req->maddr, CACHE_REQ_COHERENCE_READ));
                entry->status = _L2_WORK_READ_L2;
            } else {
                ++it_addr;
                continue;
            }
        } else if (entry->status == _L2_WORK_READ_L2) {
            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            } else if (l2_req->status() == CACHE_REQ_HIT) {
                if (dram_req) {
                    if (stats_enabled()) {
                        stats()->did_read_l2(false);
                    }
                    mh_log << "[MEM " << m_id << " @ " << system_time << " ] L2 miss is resolved for address " << start_maddr << endl;
                } else {
                    if (stats_enabled()) {
                        stats()->did_read_l2(true);
                    }
                    mh_log << "[MEM " << m_id << " @ " << system_time << " ] gets an L2 hit for address " << start_maddr << endl;
                }
                mh_assert(line);
                if (cache_rep) {
                    process_cache_rep(line, cache_rep);
                    entry->cache_rep = shared_ptr<coherenceMsg>();
                    if (!cache_req) {
                        line_info->hold = false;
                        shared_ptr<cacheRequest> new_req(new cacheRequest(start_maddr, 
                                                                          line->dirty ? CACHE_REQ_WRITE : CACHE_REQ_UPDATE, 
                                                                          m_cfg.words_per_cache_line,
                                                                          line->data,
                                                                          line_info));
                        entry->l2_req = new_req;
                        entry->status = _L2_WORK_UPDATE_L2_AND_FINISH;
                    }
                    /* spent a turn */
                    ++it_addr;
                    continue;
                } else {
                    mh_assert(cache_req);
                    uint32_t sender = cache_req->sender;
                    if (line_info->directory.count(sender)) {
                        /* have to wait for a reply first */
                        ++it_addr;
                        continue;
                    }
                    if (line_info->status == READERS) {
                        if (cache_req->type == SH_REQ) {
                            line_info->directory.insert(sender);
                            line_info->hold = false;
                            shared_ptr<cacheRequest> new_req(new cacheRequest(start_maddr, 
                                                                              line->dirty ? CACHE_REQ_WRITE : CACHE_REQ_UPDATE, 
                                                                              m_cfg.words_per_cache_line,
                                                                              line->data,
                                                                              line_info));
                            entry->l2_req = new_req;
                            entry->status = _L2_WORK_UPDATE_L2_AND_FINISH;
                            ++it_addr;
                            continue;
                        } else if (line_info->directory.empty()) {
                            /* exreq  empty */
                            line_info->status = WRITER;
                            line_info->directory.insert(sender);
                            line_info->hold = false;
                            shared_ptr<cacheRequest> new_req(new cacheRequest(start_maddr, 
                                                                              line->dirty ? CACHE_REQ_WRITE : CACHE_REQ_UPDATE, 
                                                                              m_cfg.words_per_cache_line,
                                                                              line->data,
                                                                              line_info));
                            entry->l2_req = new_req;
                            entry->status = _L2_WORK_UPDATE_L2_AND_FINISH;
                            ++it_addr;
                            continue;
                        } else {
                            for (set<uint32_t>::iterator it = line_info->directory.begin(); 
                                 it != line_info->directory.end(); ++it) 
                            {
                                shared_ptr<coherenceMsg> dir_req(new coherenceMsg);
                                dir_req->sender = m_id;
                                dir_req->receiver = *it;
                                dir_req->type = INV_REQ;
                                dir_req->maddr = start_maddr;
                                dir_req->data = shared_array<uint32_t>();
                                dir_req->did_win_last_arbitration = false;
                                entry->dir_reqs.push_back(dir_req);
                                line_info->status = WAITING_FOR_REPLIES;
                            }
                            entry->status = _L2_WORK_SEND_DIR_REQ_WAIT_DIR_REP;
                            ++it_addr;
                            continue;
                        }
                        mh_assert(false);
                    } else if (line_info->status == WRITER) {
                        mh_assert(line_info->directory.size() == 1);
                        shared_ptr<coherenceMsg> dir_req(new coherenceMsg);
                        dir_req->sender = m_id;
                        dir_req->receiver = *line_info->directory.begin();
                        dir_req->type = WB_REQ;
                        dir_req->maddr = start_maddr;
                        dir_req->data = shared_array<uint32_t>();
                        dir_req->did_win_last_arbitration = false;
                        entry->dir_reqs.push_back(dir_req);
                        line_info->status = WAITING_FOR_REPLIES;
                        entry->status = _L2_WORK_SEND_DIR_REQ_WAIT_DIR_REP;
                        ++it_addr;
                        continue;
                    }
                    /* can't reach here */
                    mh_assert(false);
                }
                mh_assert(false);
            } else {
                /* miss */
                mh_log << "[MEM " << m_id << " @ " << system_time << " ] gets a miss on L2 for address " 
                     << l2_req->maddr() << endl;
                if (line) {
                    /* the line is in L2 but hold by another entry (it wants to evict the line) */
                    l2_req->reset(); /* retry */
                    ++it_addr;
                    continue;
                }
                if (!victim) {
                    shared_ptr<dramMsg> new_msg(new dramMsg);
                    new_msg->sender = m_id;
                    new_msg->receiver = m_dram_controller_location;
                    new_msg->req = shared_ptr<dramRequest>(new dramRequest(start_maddr, DRAM_REQ_READ, m_cfg.words_per_cache_line));
                    new_msg->did_win_last_arbitration = false;
                    entry->dram_req = new_msg;
                    if (m_dram_controller_location == m_id) {
                        m_to_dram_req_schedule_q.push_back(entry->dram_req);
                    } else {
                        entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                        entry->net_msg_to_send->src = m_id;
                        entry->net_msg_to_send->dst = m_dram_controller_location;
                        entry->net_msg_to_send->type = MSG_DRAM_REQ;
                        entry->net_msg_to_send->flit_count = (1 + ADDRESS_SIZE + 
                                                              m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;
                        entry->net_msg_to_send->content = entry->dram_req;
                        m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                    }
                    entry->status = _L2_WORK_SEND_DRAM_FEED_REQ;
                    ++it_addr;
                    continue;
                } else {
                    /* we have a victim */
                    if (victim_info->status == READERS) {
                        if (victim_info->directory.empty()) {
                            if (victim->dirty) {
                                shared_ptr<dramMsg> new_msg(new dramMsg);
                                new_msg->sender = m_id;
                                new_msg->receiver = m_dram_controller_location;
                                new_msg->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                       DRAM_REQ_WRITE,
                                                                                       m_cfg.words_per_cache_line,
                                                                                       victim->data));
                                entry->dram_req = new_msg;
                                if (m_dram_controller_location == m_id) {
                                    m_to_dram_req_schedule_q.push_back(entry->dram_req);
                                } else {
                                    entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                                    entry->net_msg_to_send->src = m_id;
                                    entry->net_msg_to_send->dst = m_dram_controller_location;
                                    entry->net_msg_to_send->type = MSG_DRAM_REQ;
                                    entry->net_msg_to_send->flit_count = (1 + ADDRESS_SIZE + m_cfg.words_per_cache_line * 4 +
                                                                      m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;
                                    entry->net_msg_to_send->content = entry->dram_req;
                                    m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                                }
                                entry->status = _L2_WORK_SEND_DRAM_WRITEBACK;
                            } else {
                                entry->l2_req = shared_ptr<cacheRequest>(new cacheRequest(victim->start_maddr, CACHE_REQ_INVALIDATE));
                                entry->status = _L2_WORK_INVALID_L2_AND_RESTART;
                            }
                            ++it_addr;
                            continue;
                        }
                        for (set<uint32_t>::iterator it = victim_info->directory.begin(); 
                             it != victim_info->directory.end(); ++it) 
                        {
                            shared_ptr<coherenceMsg> dir_req(new coherenceMsg);
                            dir_req->sender = m_id;
                            dir_req->receiver = *it;
                            dir_req->type = INV_REQ;
                            dir_req->maddr = start_maddr;
                            dir_req->data = shared_array<uint32_t>();
                            dir_req->did_win_last_arbitration = false;
                            entry->dir_reqs.push_back(dir_req);
                            victim_info->status = WAITING_FOR_REPLIES;
                            entry->status = _L2_WORK_SEND_DIR_REQ_WAIT_DIR_REP;
                        }
                        ++it_addr;
                        continue;
                    } else if (victim_info->status == WRITER) {
                        mh_assert(victim_info->directory.size() == 1);
                        shared_ptr<coherenceMsg> dir_req(new coherenceMsg);
                        dir_req->sender = m_id;
                        dir_req->receiver = *victim_info->directory.begin();
                        dir_req->type = FLUSH_REQ;
                        dir_req->maddr = start_maddr;
                        dir_req->data = shared_array<uint32_t>();
                        dir_req->did_win_last_arbitration = false;
                        entry->dir_reqs.push_back(dir_req);
                        victim_info->status = WAITING_FOR_REPLIES;
                        entry->status = _L2_WORK_SEND_DIR_REQ_WAIT_DIR_REP;
                        ++it_addr;
                        continue;
                    }
                    /* can't reach here */
                    mh_assert(false);
                }
            }
        } if (entry->status == _L2_WORK_UPDATE_L2_AND_FINISH) {
            mh_assert(l2_req->status() != CACHE_REQ_MISS);
            if (stats_enabled()) {
                stats()->did_write_l2(true);
            }
            if (l2_req->status() == CACHE_REQ_HIT) {
                if (!cache_req && !cache_rep) {
                    /* finished */
                    ++m_l2_work_table_vacancy;
                    m_l2_work_table.erase(it_addr++);
                    mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] released an L2 work table entry "
                         << " for address " << start_maddr << endl;
                    continue;
                } else if (cache_req) {
                    uint32_t sender = cache_req->sender;
                    entry->dir_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                    entry->dir_rep->sender = m_id;
                    entry->dir_rep->receiver = sender;
                    entry->dir_rep->type = cache_req->type == SH_REQ ? SH_REP : EX_REP;
                    entry->dir_rep->maddr = start_maddr;
                    entry->dir_rep->data = line->data;
                    entry->dir_rep->did_win_last_arbitration = false;
                    if (sender == m_id) {
                        mh_assert(m_l1_work_table.count(start_maddr) && 
                                  m_l1_work_table[start_maddr]->status == _L1_WORK_WAIT_REP);
                        m_l1_work_table[start_maddr]->dir_rep = entry->dir_rep;
                        /* finished */
                        ++m_l2_work_table_vacancy;
                        m_l2_work_table.erase(it_addr++);
                        mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] released an L2 work table entry "
                            << " for address " << start_maddr << endl;
                        continue;
                    } else {
                        entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                        entry->net_msg_to_send->type = MSG_DIRECTORY_REQ_REP;
                        entry->net_msg_to_send->src = m_id;
                        entry->net_msg_to_send->dst = sender;
                        entry->net_msg_to_send->flit_count = (1 + ADDRESS_SIZE + 
                                                              m_cfg.words_per_cache_line * 4 +
                                                              m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;
                        entry->net_msg_to_send->content = entry->dir_rep;
                        m_to_network_schedule_q[MSG_DIRECTORY_REQ_REP].push_back(entry->net_msg_to_send);
                        entry->status = _L2_WORK_SEND_REP;
                    }
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_SEND_REP) {
            if (dir_rep->did_win_last_arbitration) {
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                dir_rep->did_win_last_arbitration = false;
                mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] has sent a directory reply to "
                     << dir_rep->receiver << " for address " << dir_rep->maddr << endl;
                if (cache_rep) {
                    entry->status = _L2_WORK_READ_L2;
                    l2_req->reset();
                    ++it_addr;
                    continue;
                } else {
                    ++m_l2_work_table_vacancy;
                    m_l2_work_table.erase(it_addr++);
                    mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] released an L2 work table entry "
                        << " for address " << start_maddr << endl;
                    continue;
                }
            } else {
                m_to_network_schedule_q[MSG_DIRECTORY_REQ_REP].push_back(entry->net_msg_to_send);
                ++it_addr;
                continue;
            }
        } else if (entry->status == _L2_WORK_SEND_DRAM_FEED_REQ) {
            if (dram_req->did_win_last_arbitration) {
                dram_req->did_win_last_arbitration = false;
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] has sent a DRAM req to " << dram_req->receiver 
                     << " for address " << dram_req->req->maddr() << endl;
                entry->status = _L2_WORK_WAIT_DRAM_FEED;
            } else {
                if (m_dram_controller_location == m_id) {
                    m_to_dram_req_schedule_q.push_back(entry->dram_req);
                } else {
                    m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_WAIT_DRAM_FEED) {
            if (dram_rep) {
                mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] has received a DRAM rep for address " 
                     << dram_rep->req->maddr() << endl;
                /* note : now assume strictly inclusive, so the initial state of the directory is all the same */
                /*        if without strictly inclusive cache, need to store and restore directory information */
                /*        to/from DRAM */
                shared_ptr<directoryCoherenceInfo> info(new directoryCoherenceInfo);
                info->status = READERS;
                info->hold = false;
                entry->l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr,
                                                                          CACHE_REQ_UPDATE, 
                                                                          m_cfg.words_per_cache_line,
                                                                          dram_rep->req->read(), info));
                entry->status = _L2_WORK_UPDATE_L2_AND_RESTART;
            } else {
                ++it_addr;
                continue;
            }
        } else if (entry->status == _L2_WORK_SEND_DRAM_WRITEBACK) {
            if (dram_req->did_win_last_arbitration) {
                if (entry->net_msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                dram_req->did_win_last_arbitration = false;
                mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] has sent a DRAM writeback to " << dram_req->receiver 
                     << " for address " << dram_req->req->maddr() << endl;
                entry->status = _L2_WORK_INVALID_L2_AND_RESTART;
            } else {
                if (m_dram_controller_location == m_id) {
                    m_to_dram_req_schedule_q.push_back(entry->dram_req);
                } else {
                    m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L2_WORK_INVALID_L2_AND_RESTART) {
            mh_assert(l2_req->status() != CACHE_REQ_MISS);
            if (l2_req->status() == CACHE_REQ_HIT) {
                entry->l2_req = shared_ptr<cacheRequest>(new cacheRequest(cache_req->maddr, CACHE_REQ_COHERENCE_READ));
                entry->status = _L2_WORK_READ_L2;
            } else {
                ++it_addr;
                continue;
            }
        } else if (entry->status == _L2_WORK_SEND_DIR_REQ_WAIT_DIR_REP) {
            shared_ptr<cacheLine> _line = line ? line : victim;
            shared_ptr<directoryCoherenceInfo> _info = line ? line_info : victim_info;
            if (cache_rep) {
                process_cache_rep(_line, cache_rep);
                entry->cache_rep = shared_ptr<coherenceMsg>();
                if (line) {
                    bool finished = false;
                    if (cache_req->type == SH_REQ) {
                        if (line_info->status == READERS) {
                            finished = true;
                        }
                    } else {
                        if (line_info->directory.empty()) {
                            finished = true;
                        }
                    }
                    if (finished) {
                        /* no need to send more requests */
                        entry->dir_reqs.clear();
                        if (entry->net_msg_to_send) {
                            entry->net_msg_to_send = shared_ptr<message_t>();
                        }
                        entry->status = _L2_WORK_READ_L2;
                        ++it_addr;
                        continue;
                    }
                } else {
                    /* victim */
                    if (victim_info->directory.empty()) {
                        /* no need to send more requests */
                        entry->dir_reqs.clear();
                        if (entry->net_msg_to_send) {
                            entry->net_msg_to_send = shared_ptr<message_t>();
                        }

                        if (victim->dirty) {
                            shared_ptr<dramMsg> new_msg(new dramMsg);
                            new_msg->sender = m_id;
                            new_msg->receiver = m_dram_controller_location;
                            new_msg->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                   DRAM_REQ_WRITE,
                                                                                   m_cfg.words_per_cache_line,
                                                                                   victim->data));
                            entry->dram_req = new_msg;
                            if (m_dram_controller_location == m_id) {
                                m_to_dram_req_schedule_q.push_back(entry->dram_req);
                            } else {
                                if (!entry->net_msg_to_send) {
                                    entry->net_msg_to_send = shared_ptr<message_t>(new message_t);
                                    entry->net_msg_to_send->src = m_id;
                                    entry->net_msg_to_send->dst = m_dram_controller_location;
                                    entry->net_msg_to_send->type = MSG_DRAM_REQ;
                                    entry->net_msg_to_send->flit_count = (1 + ADDRESS_SIZE + m_cfg.words_per_cache_line * 4 +
                                                                          m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;
                                    entry->net_msg_to_send->content = entry->dram_req;
                                }
                                m_to_network_schedule_q[MSG_DRAM_REQ].push_back(entry->net_msg_to_send);
                            }
                            entry->status = _L2_WORK_SEND_DRAM_WRITEBACK;
                        } else {
                            entry->l2_req = shared_ptr<cacheRequest>(new cacheRequest(victim->start_maddr, CACHE_REQ_INVALIDATE));
                            entry->status = _L2_WORK_INVALID_L2_AND_RESTART;
                        }
                        ++it_addr;
                        continue;
                    }
                }
            }
            while (entry->dir_reqs.size()) {
                /* this belongs to the previous cycle */
                shared_ptr<coherenceMsg> dir_req = entry->dir_reqs.front();
                if (dir_req->did_win_last_arbitration) {
                    mh_log << "[DIRECTORY " << m_id << " @ " << system_time << " ] has sent a directory request to " << dir_req->receiver
                         << " for address " << dir_req->maddr << endl;
                    if (entry->net_msg_to_send) {
                        entry->net_msg_to_send = shared_ptr<message_t>();
                    }
                    dir_req->did_win_last_arbitration = false;
                    entry->dir_reqs.erase(entry->dir_reqs.begin());
                } else {
                    break;
                }
            }
            while (entry->dir_reqs.size()) {
                shared_ptr<coherenceMsg> dir_req = entry->dir_reqs.front();
                if (_info->directory.count(dir_req->receiver) == 0) {
                    if (entry->net_msg_to_send) {
                        entry->net_msg_to_send = shared_ptr<message_t>();
                    }
                    entry->dir_reqs.erase(entry->dir_reqs.begin());
                } else {
                    break;
                }
            }
            if (entry->dir_reqs.size()) {
                shared_ptr<coherenceMsg> dir_req = entry->dir_reqs.front();
                if (dir_req->receiver == m_id) {
                    m_to_cache_req_schedule_q.push_back(make_tuple(false, dir_req));
                } else {
                    if (!entry->net_msg_to_send) {
                        shared_ptr<message_t> new_msg(new message_t);
                        new_msg->type = MSG_DIRECTORY_REQ_REP;
                        new_msg->src = m_id;
                        new_msg->dst = dir_req->receiver;
                        new_msg->flit_count =  (1 + ADDRESS_SIZE + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;
                        new_msg->content = dir_req;
                        entry->net_msg_to_send = new_msg;
                    }
                    m_to_network_schedule_q[MSG_DIRECTORY_REQ_REP].push_back(entry->net_msg_to_send);
                }
            }
            ++it_addr;
            continue;
        }
    }

}

void privateSharedMSI::dram_work_table_update() {
    for (toDRAMTable::iterator it_addr = m_dram_work_table.begin(); it_addr != m_dram_work_table.end(); ) {
        shared_ptr<toDRAMEntry> entry = it_addr->second;
        if (entry->dram_req->req->status() == DRAM_REQ_DONE) {
            if (!entry->dram_rep) {
                entry->dram_rep = shared_ptr<dramMsg>(new dramMsg);
                entry->dram_rep->sender = m_id;
                entry->dram_rep->req = entry->dram_req->req;
                entry->dram_rep->did_win_last_arbitration = false;
            }
            if (entry->dram_req->sender == m_id) {
                /* guaranteed to have an active entry in l2 work table */
                // TODO : maybe not. write and forget case ? 
                maddr_t start_maddr = entry->dram_req->req->maddr();
                mh_assert(m_l2_work_table.count(start_maddr) && m_l2_work_table[start_maddr]->status == _L2_WORK_WAIT_DRAM_FEED);
                m_l2_work_table[start_maddr]->dram_rep = entry->dram_rep;
                m_dram_work_table.erase(it_addr++);
                mh_log << "[DRAM " << m_id << " @ " << system_time << " ] has sent a dram rep for address " << entry->dram_rep->req->maddr()
                     << " to core " << m_id << endl;
                continue;
            } else {
                if (!entry->dram_rep->did_win_last_arbitration) {
                    if (!entry->net_msg_to_send) {
                        shared_ptr<message_t> new_msg(new message_t);
                        new_msg->type = MSG_DRAM_REP;
                        new_msg->src = m_id;
                        new_msg->dst = entry->dram_req->sender;
                        uint32_t data_size = entry->dram_rep->req->is_read() ? m_cfg.words_per_cache_line * 4 : 0;
                        new_msg->flit_count = (1 + ADDRESS_SIZE + data_size + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;
                        new_msg->content = entry->dram_rep;
                        entry->net_msg_to_send = new_msg;
                    }
                    m_to_network_schedule_q[MSG_DRAM_REP].push_back(entry->net_msg_to_send);
                } else {
                    m_dram_work_table.erase(it_addr++);
                    if (entry->net_msg_to_send) {
                        entry->net_msg_to_send = shared_ptr<message_t>();
                    }
                    mh_log << "[DRAM " << m_id << " @ " << system_time << " ] has sent a dram rep for address " 
                         << entry->dram_rep->req->maddr() << " to core " << entry->dram_req->sender << endl;
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
            if (cc_msg->type == SH_REP || cc_msg->type == EX_REP) {
                /* guaranteed to accept */
                mh_assert(m_l1_work_table.count(cc_msg->maddr) && 
                          m_l1_work_table[cc_msg->maddr]->status == _L1_WORK_WAIT_REP &&
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
            dram_msg->did_win_last_arbitration = false;
            m_to_dram_req_schedule_q.push_back(dram_msg);
            break;
        }
    }

    if (m_core_receive_queues[MSG_DRAM_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DRAM_REP]->front();
        shared_ptr<dramMsg> dram_msg = static_pointer_cast<dramMsg>(msg->content);
        maddr_t start_maddr = dram_msg->req->maddr(); /* always access by a cache line */
        if (m_l2_work_table.count(start_maddr) > 0 && m_l2_work_table[start_maddr]->status == _L2_WORK_WAIT_DRAM_FEED) {
            m_l2_work_table[start_maddr]->dram_rep = dram_msg;
        }
        /* write back is ignored */
        m_core_receive_queues[MSG_DRAM_REP]->pop();
    }

}

void privateSharedMSI::schedule_requests() {

    /* random arbitration */
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    set<maddr_t> issued_start_maddrs; 

    /* 1 : arbitrates requests from the core. */
    /*     the core is assumed to have a finite number of access ports to the memory */
    /*     this ports are hold by accepted requests until it is eventually served    */
    random_shuffle(m_core_port_schedule_q.begin(), m_core_port_schedule_q.end(), rr_fn);
    uint32_t count = 0;
    while (m_core_port_schedule_q.size()) {
        if (count < m_available_core_ports) {
            m_to_cache_req_schedule_q.push_back(make_tuple(true, m_core_port_schedule_q.front()));
            ++count;
        } else {
            /* sorry */
            set_req_status(m_core_port_schedule_q.front(), REQ_RETRY);
        }
        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
    }
    
    /* 2 : arbitrates l1 work table for new entries */
    random_shuffle(m_to_cache_req_schedule_q.begin(), m_to_cache_req_schedule_q.end(), rr_fn);
    while (m_l1_work_table_vacancy > 0 && m_to_cache_req_schedule_q.size()) {
        bool is_core_req = m_to_cache_req_schedule_q.front().get<0>();
        if (is_core_req) {
            shared_ptr<memoryRequest> req = 
                static_pointer_cast<memoryRequest>(m_to_cache_req_schedule_q.front().get<1>());
            maddr_t start_maddr = req->maddr();
            if (issued_start_maddrs.count(start_maddr) || m_l1_work_table.count(start_maddr)) {
                set_req_status(req, REQ_RETRY);
                continue;
            }
            issued_start_maddrs.insert(start_maddr);

            shared_ptr<toL1Entry> new_entry(new toL1Entry);
            new_entry->status = _L1_WORK_WAIT_L1;
            new_entry->core_req = req;

            shared_ptr<cacheRequest> l1_req
                (new cacheRequest(req->maddr(), 
                                  req->is_read()? CACHE_REQ_COHERENCE_READ : CACHE_REQ_COHERENCE_WRITE,
                                  req->word_count(),
                                  req->is_read()? shared_array<uint32_t>() : req->data()));
            new_entry->l1_req = l1_req;
           
            shared_ptr<catRequest> cat_req(new catRequest(req->maddr(), m_id));
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
            shared_ptr<coherenceMsg> msg = 
                static_pointer_cast<coherenceMsg>(m_to_cache_req_schedule_q.front().get<1>());
            maddr_t start_maddr = msg->maddr;
            if (issued_start_maddrs.count(start_maddr) || m_l1_work_table.count(start_maddr)) {
                m_to_cache_req_schedule_q.erase(m_to_cache_req_schedule_q.begin());
                continue;
            }
            issued_start_maddrs.insert(start_maddr);

            shared_ptr<toL1Entry> new_entry(new toL1Entry);
            new_entry->status = _L1_WORK_WAIT_L1;
            new_entry->dir_req = msg;

            mh_log << "[L1 " << m_id << " @ " << system_time << " ] received a directory request " << endl;

            shared_ptr<cacheRequest> l1_req
                (new cacheRequest(msg->maddr, msg->type == WB_REQ? CACHE_REQ_COHERENCE_READ : CACHE_REQ_COHERENCE_INVALIDATE));
            new_entry->l1_req = l1_req;

            new_entry->core_req = shared_ptr<memoryRequest>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->cat_req = shared_ptr<catRequest>();
            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->cache_rep = shared_ptr<coherenceMsg>();
            new_entry->requested_time = system_time;
            new_entry->net_msg_to_send = shared_ptr<message_t>();

            msg->did_win_last_arbitration = true;

            m_l1_work_table[start_maddr] = new_entry;
            --m_l1_work_table_vacancy;
            
            m_to_cache_req_schedule_q.erase(m_to_cache_req_schedule_q.begin());

        }
    }
    issued_start_maddrs.clear();
    m_to_cache_req_schedule_q.clear();

    /* 3 : arbitrate inputs to l2 work table */
    random_shuffle(m_to_directory_rep_schedule_q.begin(), m_to_directory_rep_schedule_q.end(), rr_fn);
    while (m_to_directory_rep_schedule_q.size()) {
        shared_ptr<coherenceMsg> msg = m_to_directory_rep_schedule_q.front();
        maddr_t start_maddr = msg->maddr;
        if (m_l2_work_table.count(start_maddr)) {
            /* always into the head */
            if (!m_l2_work_table[start_maddr]->cache_rep) {
                m_l2_work_table[start_maddr]->cache_rep = msg;
                msg->did_win_last_arbitration = true;
            }
        } else if (m_l2_work_table_vacancy) {
            shared_ptr<toL2Entry> new_entry(new toL2Entry);
            new_entry->status = _L2_WORK_READ_L2;
            new_entry->cache_rep = msg;

            shared_ptr<cacheRequest> l2_req(new cacheRequest(msg->maddr, CACHE_REQ_COHERENCE_READ));
            new_entry->l2_req = l2_req;

            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->dram_req = shared_ptr<dramMsg>();
            new_entry->dram_rep = shared_ptr<dramMsg>();

            msg->did_win_last_arbitration = true;

            m_l2_work_table[start_maddr] = new_entry;
            --m_l2_work_table_vacancy;
        }
        m_to_directory_rep_schedule_q.erase(m_to_directory_rep_schedule_q.begin());
    }
    random_shuffle(m_to_directory_req_schedule_q.begin(), m_to_directory_req_schedule_q.end(), rr_fn);
    while (m_to_directory_req_schedule_q.size()) {
        shared_ptr<coherenceMsg> msg = m_to_directory_req_schedule_q.front();
        maddr_t start_maddr = msg->maddr;
        if (m_l2_work_table.count(start_maddr) == 0 && m_l2_work_table_vacancy > 0) {
            shared_ptr<toL2Entry> new_entry(new toL2Entry);
            new_entry->status = _L2_WORK_READ_L2;
            new_entry->cache_req = msg;

            shared_ptr<cacheRequest> l2_req(new cacheRequest(msg->maddr, CACHE_REQ_COHERENCE_READ));
            new_entry->l2_req = l2_req;

            new_entry->cache_rep = shared_ptr<coherenceMsg>();
            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->dram_req = shared_ptr<dramMsg>();
            new_entry->dram_rep = shared_ptr<dramMsg>();

            msg->did_win_last_arbitration = true;

            m_l2_work_table[start_maddr] = new_entry;
            --m_l2_work_table_vacancy;
        }
        m_to_directory_req_schedule_q.erase(m_to_directory_req_schedule_q.begin());
    }
    
    /* 4 : arbitrate inputs to dram work table */
    random_shuffle(m_to_dram_req_schedule_q.begin(), m_to_dram_req_schedule_q.end(), rr_fn);
    while (m_to_dram_req_schedule_q.size()) {
        mh_assert(m_dram_controller);
        shared_ptr<dramMsg> msg = m_to_dram_req_schedule_q.front();
        if (m_dram_controller->available()) {
            mh_assert(!m_dram_work_table.count(msg->req->maddr()));
            shared_ptr<toDRAMEntry> new_entry(new toDRAMEntry);
            new_entry->dram_req = msg;
            new_entry->dram_rep = shared_ptr<dramMsg>();
            new_entry->net_msg_to_send = shared_ptr<message_t>();
            m_dram_work_table[msg->req->maddr()] = new_entry;
            msg->did_win_last_arbitration = true;
            m_dram_controller->request(msg->req);
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
                l1_req->request_type() == CACHE_REQ_COHERENCE_WRITE) {
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
                l2_req->request_type() == CACHE_REQ_COHERENCE_WRITE) {
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
        m_cat_req_schedule_q.erase(m_cat_req_schedule_q.begin());
    }
    m_cat_req_schedule_q.clear();

    /* l1 read requests */
    random_shuffle(m_l1_read_req_schedule_q.begin(), m_l1_read_req_schedule_q.end(), rr_fn);
    while (m_l1->read_port_available() && m_l1_read_req_schedule_q.size()) {
        m_l1->request(m_l1_read_req_schedule_q.front());
        m_l1_read_req_schedule_q.erase(m_l1_read_req_schedule_q.begin());
    }
    m_l1_read_req_schedule_q.clear();
    
    /* l1 write requests */
    random_shuffle(m_l1_write_req_schedule_q.begin(), m_l1_write_req_schedule_q.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q.size()) {
        m_l1->request(m_l1_write_req_schedule_q.front());
        m_l1_write_req_schedule_q.erase(m_l1_write_req_schedule_q.begin());
    }
    m_l1_write_req_schedule_q.clear();

    /* l2 read requests */
    random_shuffle(m_l2_read_req_schedule_q.begin(), m_l2_read_req_schedule_q.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q.size()) {
        shared_ptr<cacheRequest> req = m_l2_read_req_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(req->maddr());
        if (issued_start_maddrs.count(start_maddr) == 0) {
            m_l2->request(req);
            issued_start_maddrs.insert(start_maddr);
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
        m_l2_write_req_schedule_q.erase(m_l2_write_req_schedule_q.begin());
    }
    m_l2_write_req_schedule_q.clear();

    /* networks */
    for (uint32_t it_channel = 0; it_channel < NUM_MSG_TYPES; ++it_channel) {
        random_shuffle(m_to_network_schedule_q[it_channel].begin(), m_to_network_schedule_q[it_channel].end(), rr_fn);
        while (m_to_network_schedule_q[it_channel].size()) {
            shared_ptr<message_t> msg = m_to_network_schedule_q[it_channel].front();
            if (m_core_send_queues[it_channel]->push_back(msg)) {
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

