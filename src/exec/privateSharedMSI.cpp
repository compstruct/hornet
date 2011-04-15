// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSI.hpp"
#include "messages.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>

#define DEBUG

#ifdef DEBUG
#define mh_assert(X) assert(X)
#else
#define mh_assert(X)
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
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<privateSharedMSI::directoryCoherenceInfo>(new privateSharedMSI::directoryCoherenceInfo);
    }
    shared_ptr<privateSharedMSI::directoryCoherenceInfo> info = 
        static_pointer_cast<privateSharedMSI::directoryCoherenceInfo>(line.coherence_info);
    info->status = privateSharedMSI::WAITING_FOR_DRAM;
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
    m_to_l1_table_local_quota(cfg.max_local_cache_requests_in_flight),
    m_to_l1_table_remote_quota(cfg.max_remote_cache_requests_in_flight),
    m_to_l2_table_local_quota(cfg.max_local_directory_requests_in_flight),
    m_to_l2_table_remote_quota(cfg.max_remote_directory_requests_in_flight)
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

bool privateSharedMSI::available() { return m_to_l1_table_local_quota > 0; }

uint32_t privateSharedMSI::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

void privateSharedMSI::request(shared_ptr<memoryRequest> req) {

    /* max number of in-flight memory requests */
    mh_assert(available());

    /* assumes a request is not across multiple cache lines */
    uint32_t __attribute__((unused)) byte_offset = req->maddr().address%(m_cfg.words_per_cache_line*4);
    mh_assert( (byte_offset + req->word_count()*4) <= m_cfg.words_per_cache_line * 4);

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    shared_ptr<toL1Entry> new_entry(new toL1Entry);
    new_entry->status = _TO_L1_WAIT_L1;
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

    m_to_l1_req_schedule_q.push_back(new_entry);

    --m_to_l1_table_local_quota;
    
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

    to_l1_table_update();

    to_l2_table_update();

}

void privateSharedMSI::to_l1_table_update() {
    for (toL1Table::iterator it_addr = m_to_l1_table.begin(); it_addr != m_to_l1_table.end(); ++it_addr) {
        maddr_t start_maddr = it_addr->first;
        toL1Queue &q = it_addr->second;
        if (q.empty()) {
            m_to_l1_table.erase(it_addr);
            continue;
        }
        shared_ptr<toL1Entry> entry = q.front();
        if (entry->status == _TO_L1_WAIT_L1) {
            mh_assert(!entry->dir_rep);
            shared_ptr<cacheRequest> l1_req = entry->l1_req;
            if (l1_req->status() == CACHE_REQ_HIT) {
                if (entry->core_req) {
                    /* was a core request */
                    if (stats_enabled()) {
                        if (entry->core_req->is_read()) {   
                            stats()->did_read_l1(true);
                            stats()->did_finish_read(system_time - entry->requested_time);
                        } else {
                            stats()->did_write_l1(true);
                            stats()->did_finish_write(system_time - entry->requested_time);
                        }
                    }
                    set_req_status(entry->core_req, REQ_DONE);
                    /* _TO_L1_END */
                    q.erase(q.begin());
                    if (!entry->core_req) {
                        ++m_to_l1_table_local_quota;
                    } else {
                        ++m_to_l1_table_remote_quota;
                    }
                    continue;
                } else {
                    /* was a directory request */
                    shared_ptr<coherenceMsg> dir_req = entry->dir_req;
                    shared_ptr<cacheLine> line = l1_req->line_copy();
                    if (dir_req->type == INV_REQ) {
                       mh_assert(!line->dirty);
                        if (line) {
                            shared_ptr<coherenceMsg> rep(new coherenceMsg);
                            rep->sender = m_id;
                            rep->type = INV_REP;
                            rep->maddr = start_maddr;
                            rep->data = shared_array<uint32_t>();
                            entry->cache_rep = rep;
                            entry->status = _TO_L1_SEND_REP;
                        } else {
                            /* the line was already invalidated. discard the request */
                            /* _TO_L1_END */
                            q.erase(q.begin());
                            if (!entry->core_req) {
                                ++m_to_l1_table_local_quota;
                            } else {
                                ++m_to_l1_table_remote_quota;
                            }
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
                        entry->cache_rep = rep;
                        entry->status = _TO_L1_SEND_REP;
                    }
                }
            } else if (entry->l1_req->status() == CACHE_REQ_MISS) {
                shared_ptr<cacheLine> victim = l1_req->victim_line_copy();
                if (!victim) {
                    /* an empty line is rerserved (pending stats) */
                    entry->status = _TO_L1_WAIT_CAT;
                } else {
                    mh_assert(victim->coherence_info);
                    shared_ptr<cacheCoherenceInfo> info = static_pointer_cast<cacheCoherenceInfo>(victim->coherence_info);
                    if (cache_can_evict_line(*victim)) {
                        shared_ptr<coherenceMsg> rep(new coherenceMsg);
                        rep->sender = m_id;
                        rep->type = info->status == MODIFIED ? FLUSH_REP : WB_REP;
                        rep->maddr = start_maddr;
                        rep->data = victim->data;
                        entry->cache_rep = rep;
                        entry->status = _TO_L1_SEND_REP;
                    } else {
                        /* couldn't invalidate - retry */
                        entry->l1_req->reset(); /* the scheduler will try to make an L1 request again in the next cycle */
                        entry->status = _TO_L1_WAIT_L1;
                    }
                }
                cerr << "[MEM " << m_id << " @ " << system_time << " ] req from core is missed on L1 for start address "
                     << hex << start_maddr<< dec << endl;
            }
            continue;
        } else if (entry->status == _TO_L1_WAIT_REP) {
            if (entry->dir_rep) {
                shared_ptr<cacheCoherenceInfo> info(new cacheCoherenceInfo);
                info->directory_home = entry->cat_req->home();
                info->status = entry->dir_rep->type == SH_REP ? SHARED : MODIFIED;
                shared_ptr<cacheRequest> l1_req(new cacheRequest(start_maddr, CACHE_REQ_WRITE, m_cfg.words_per_cache_line,
                                                                 entry->dir_rep->data, info));
                entry->l1_req = l1_req;
                entry->status = _TO_L1_FEED_L1;
            }
            continue;
        } else if (entry->status == _TO_L1_FEED_L1) {
            mh_assert(entry->l1_req->status() != CACHE_REQ_MISS);
            if (entry->l1_req->status() == CACHE_REQ_HIT) {
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
                /* _TO_L1_END */
                q.erase(q.begin());
                ++m_to_l1_table_local_quota;
                continue;
            }
            continue;
        }
        if (entry->status == _TO_L1_WAIT_CAT) {
            /* note: do not else this if - it includes the case from _TO_L1_WAIT_L1 at the same cycle */
            mh_assert(entry->cat_req);
            mh_assert(entry->core_req);
            if (entry->cat_req->status() != CAT_REQ_DONE) {
                continue;
            } else {
                shared_ptr<coherenceMsg> req(new coherenceMsg);
                req->sender = m_id;
                req->type = entry->core_req->is_read() ? SH_REQ : EX_REQ;
                req->maddr = start_maddr;
                req->data = shared_array<uint32_t>();
                entry->cache_req = req;
                entry->status = _TO_L1_SEND_REQ;
            }
        }
        if (entry->status == _TO_L1_SEND_REQ) {
            /* note: do not else this if - it includes the case from _TO_L1_WAIT_L1 at the same cycle */
            bool sent = false;
            uint32_t dir_home = entry->cat_req->home();
            if (dir_home != m_id) {
                shared_ptr<message_t> msg(new message_t);
                msg->type = MSG_CACHE_REQ;
                msg->src = m_id;
                msg->dst = dir_home;
                msg->flit_count = (1 + 8 + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;;
                msg->content = entry->cache_req;
                sent = m_core_send_queues[MSG_CACHE_REP]->push_back(msg);
            } else {
                if (m_to_l2_table_local_quota > 0) {
                    sent = true;
                    shared_ptr<toL2Entry> new_entry(new toL2Entry);
                    new_entry->status = _TO_L2_READ_L2;
                    new_entry->cache_req = entry->cache_req;

                    shared_ptr<cacheRequest> l2_req(new cacheRequest(entry->cache_req->maddr, 
                                                                     CACHE_REQ_COHERENCE_READ,
                                                                     m_cfg.words_per_cache_line));
                    new_entry->l2_req = l2_req;

                    m_to_l2_req_schedule_q.push_back(new_entry);
                    --m_to_l2_table_local_quota;
                }
            }
            if (sent) {
                cerr << "[MEM " << m_id << " @ " << system_time << " ] has sent a cache req to directory at " 
                     << dir_home << " for address " << hex << start_maddr<< dec << endl;
                entry->status = _TO_L1_WAIT_REP;
            }
        } 
        if (entry->status == _TO_L1_SEND_REP) {
            /* note: do not else this if - it includes the case from _TO_L1_WAIT_L1 at the same cycle */
            bool sent = false;
            mh_assert(!entry->l1_req);
            shared_ptr<cacheLine> line = entry->l1_req->line_copy();
            mh_assert(!line);
            uint32_t dir_home = static_pointer_cast<cacheCoherenceInfo>(line->coherence_info)->directory_home;
            if (dir_home != m_id) {
                shared_ptr<message_t> msg(new message_t);
                msg->type = MSG_CACHE_REP;
                msg->src = m_id;
                msg->dst = dir_home;
                uint32_t bytes = 1 + 8; /* 1 for message 8 for address */
                if (entry->cache_rep->type != INV_REP) {
                    bytes += m_cfg.words_per_cache_line * 4;
                }
                msg->flit_count = (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit;;
                msg->content = entry->cache_rep;
                sent = m_core_send_queues[MSG_CACHE_REP]->push_back(msg);
            } else {
                m_local_cache_reps_this_cycle.push_back(entry->cache_rep);

                /* guaranteed to have an active entry in the to-L2 table */
                sent = true;
            }
            if (sent) {
                if (entry->core_req) {
                    /* invRep/flushRep message for the victim line is sent. now sending a cache request */
                    entry->status = _TO_L1_WAIT_CAT;
                } else if (!entry->dir_req) {
                    LOG(log,4) << "[L1 " << m_id << " @ " << system_time << " ] sends back a cc reply for start address "
                        << hex << entry->cache_rep->maddr<< dec << endl;
                    q.erase(q.begin());
                    if (!entry->core_req) {
                        ++m_to_l1_table_local_quota;
                    } else {
                        ++m_to_l1_table_remote_quota;
                    }
                    continue;
                }
            } else {
                continue;
            }
        }
    }

}

void privateSharedMSI::to_l2_table_update() {
}

void privateSharedMSI::accept_incoming_messages() {
    if (m_core_receive_queues[MSG_DIRECTORY_REQ_REP]->size() > 0 && m_to_l1_table_remote_quota > 0) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_DIRECTORY_REQ_REP]->front();
        shared_ptr<coherenceMsg> cc_msg = static_pointer_cast<coherenceMsg>(msg->content);
        if (cc_msg->type == INV_REQ || cc_msg->type == FLUSH_REQ || cc_msg->type == WB_REQ) {   
            /* directory request cast */
            shared_ptr<toL1Entry> new_entry(new toL1Entry);
            new_entry->status = _TO_L1_WAIT_L1;
            new_entry->core_req = shared_ptr<memoryRequest>();
            new_entry->dir_req = cc_msg;

            shared_ptr<cacheRequest> l1_req
                (new cacheRequest(cc_msg->maddr, 
                                  cc_msg->type == WB_REQ? CACHE_REQ_COHERENCE_INVALIDATE : CACHE_REQ_COHERENCE_READ));
            new_entry->l1_req = l1_req;

            new_entry->dir_rep = shared_ptr<coherenceMsg>();
            new_entry->cat_req = shared_ptr<catRequest>();
            new_entry->cache_req = shared_ptr<coherenceMsg>();
            new_entry->cache_rep = shared_ptr<coherenceMsg>();
            new_entry->requested_time = system_time;

            m_to_l1_req_schedule_q.push_back(new_entry);

            --m_to_l1_table_remote_quota;
        } else {
            /* directory reply case */
            maddr_t &start_maddr = cc_msg->maddr;
            mh_assert(m_to_l1_table.count(start_maddr) > 0 && !m_to_l1_table[start_maddr].empty());
            m_to_l1_table[start_maddr].front()->dir_rep = cc_msg;
        }
    }
    if (m_core_receive_queues[MSG_CACHE_REQ]->size() > 0 && m_to_l2_table_remote_quota > 0) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_CACHE_REQ]->front();
        shared_ptr<coherenceMsg> cc_msg = static_pointer_cast<coherenceMsg>(msg->content);
        shared_ptr<toL2Entry> new_entry(new toL2Entry);
        new_entry->status = _TO_L2_READ_L2;
        new_entry->cache_req = cc_msg;

        shared_ptr<cacheRequest> l2_req(new cacheRequest(cc_msg->maddr, CACHE_REQ_COHERENCE_READ));
        new_entry->l2_req = l2_req;

        new_entry->cache_rep = shared_ptr<coherenceMsg>();
        new_entry->dram_req = shared_ptr<dramRequest>();

        m_to_l2_req_schedule_q.push_back(new_entry);

        --m_to_l2_table_remote_quota;
    }
}

void privateSharedMSI::schedule_requests() {

    /* random arbitration */
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    set<maddr_t> issued_start_maddrs; /* Only one request for the same cache line */

    /* to-L1 table requests (core requests & inv/wb/flushReq) */
    random_shuffle(m_to_l1_req_schedule_q.begin(), m_to_l1_req_schedule_q.end(), rr_fn);
    while (!m_to_l1_req_schedule_q.empty()) {
        shared_ptr<toL1Entry> entry = m_to_l1_req_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(entry->core_req->maddr());
        m_to_l1_table[start_maddr].push_back(entry);
        m_to_l1_req_schedule_q.erase(m_to_l1_req_schedule_q.begin());
    }

    /* to-L2 table requests (sh/exReq) */
    random_shuffle(m_to_l2_req_schedule_q.begin(), m_to_l2_req_schedule_q.end(), rr_fn);
    while (!m_to_l2_req_schedule_q.empty()) {
        shared_ptr<toL2Entry> entry = m_to_l2_req_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(entry->cache_req->maddr);
        m_to_l2_table[start_maddr].push_back(entry);
        m_to_l2_req_schedule_q.erase(m_to_l2_req_schedule_q.begin());
    }

    /* try make cache requests */
    for (toL1Table::iterator it_addr = m_to_l1_table.begin(); it_addr != m_to_l1_table.end(); ++it_addr) {
        maddr_t start_maddr = it_addr->first;
        toL1Queue &q = it_addr->second;
        if (q.empty()) {
            m_to_l1_table.erase(it_addr);
            continue;
        }
        if (q.front()->l1_req && q.front()->l1_req->status() == CACHE_REQ_NEW) {
            shared_ptr<cacheRequest> l1_req = q.front()->l1_req;
            /* not requested yet */
            if (l1_req->request_type() == CACHE_REQ_WRITE || l1_req->request_type() == CACHE_REQ_COHERENCE_WRITE) {
                m_l1_write_req_schedule_q.push_back(l1_req);
            } else {
                m_l1_read_req_schedule_q.push_back(l1_req);
            }
        }
        if (q.front()->cat_req && q.front()->cat_req->status() == CAT_REQ_NEW) {
                m_cat_req_schedule_q.push_back(q.front()->cat_req);
        }
    }
    for (toL2Table::iterator it_addr = m_to_l2_table.begin(); it_addr != m_to_l2_table.end(); ++it_addr) {
        maddr_t start_maddr = it_addr->first;
        toL2Queue &q = it_addr->second;
        if (q.empty()) {
            m_to_l2_table.erase(it_addr);
            continue;
        }
        if (q.front()->l2_req && q.front()->l2_req->status() == CACHE_REQ_NEW) {
            shared_ptr<cacheRequest> l2_req = q.front()->l2_req;
            /* not requested yet */
            if (l2_req->request_type() == CACHE_REQ_WRITE || l2_req->request_type() == CACHE_REQ_COHERENCE_WRITE) {
                m_l2_write_req_schedule_q.push_back(l2_req);
            } else {
                m_l2_read_req_schedule_q.push_back(l2_req);
            }
        }
    }


    /* cat requests */
    random_shuffle(m_cat_req_schedule_q.begin(), m_cat_req_schedule_q.end(), rr_fn);
    while (m_cat->available() && m_cat_req_schedule_q.size() > 0) {
        m_cat->request(m_cat_req_schedule_q.front());
        m_cat_req_schedule_q.erase(m_cat_req_schedule_q.begin());
    }
    m_cat_req_schedule_q.clear();

    /* l1 read requests */
    random_shuffle(m_l1_read_req_schedule_q.begin(), m_l1_read_req_schedule_q.end(), rr_fn);
    while (m_l1->read_port_available() && m_l1_read_req_schedule_q.size() > 0) {
        m_l1->request(m_l1_read_req_schedule_q.front());
        m_l1_read_req_schedule_q.erase(m_l1_read_req_schedule_q.begin());
    }
    m_l1_read_req_schedule_q.clear();
    
    /* l1 write requests */
    random_shuffle(m_l1_write_req_schedule_q.begin(), m_l1_write_req_schedule_q.end(), rr_fn);
    while (m_l1->write_port_available() && m_l1_write_req_schedule_q.size() > 0) {
        m_l1->request(m_l1_write_req_schedule_q.front());
        m_l1_write_req_schedule_q.erase(m_l1_write_req_schedule_q.begin());
    }
    m_l1_write_req_schedule_q.clear();

    /* l2 read requests */
    random_shuffle(m_l2_read_req_schedule_q.begin(), m_l2_read_req_schedule_q.end(), rr_fn);
    while (m_l2->read_port_available() && m_l2_read_req_schedule_q.size() > 0) {
        shared_ptr<cacheRequest> req = m_l2_read_req_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(req->maddr());
        if (issued_start_maddrs.count(start_maddr) == 0) {
            m_l2->request(req);
            issued_start_maddrs.insert(start_maddr);
        }
        m_l2_read_req_schedule_q.erase(m_l2_read_req_schedule_q.begin());
    }
    issued_start_maddrs.clear();
    
    /* dram requests */
    random_shuffle(m_dram_req_schedule_q.begin(), m_dram_req_schedule_q.end(), rr_fn);
    while (m_dram_req_schedule_q.size() > 0 && m_dram_controller->available()) {
        m_dram_controller->request(m_dram_req_schedule_q.front());
        m_dram_req_schedule_q.erase(m_dram_req_schedule_q.begin());
    }
    m_dram_req_schedule_q.clear();

}

