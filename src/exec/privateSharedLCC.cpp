// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedLCC.hpp"
#include "messages.hpp"
#include <boost/function.hpp>
#include <boost/bind.hpp>

#define TIMESTAMP_WORDS 2

#define PRIVATE_BYPASS
#undef PRIVATE_BYPASS

#define MAX_IDEAL 10000
#undef MAX_IDEAL

#define PRINT_PROGRESS
//#undef PRINT_PROGRESS

#define PRINT_IDEAL 
#undef PRINT_IDEAL

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

#define WATERTINY

#define MR 10000
#define LR 40

#ifdef FFTTINY
#define W1 60
#define W2 280
#endif

#ifdef LUTINY
#define W1 20
#define W2 110
#endif

#ifdef OCEANTINY
#define W1 130
#define W2 420
#endif

#ifdef RADIXTINY
#define W1 10
#define W2 20
#endif

#ifdef WATERTINY
#define W1 680
#define W2 4800
#endif

#ifndef W1
#define W1 230
#define W2 2000
#endif

static shared_ptr<void> copy_coherence_info(shared_ptr<void> source) { 
    shared_ptr<privateSharedLCC::coherenceInfo> ret
        (new privateSharedLCC::coherenceInfo(*static_pointer_cast<privateSharedLCC::coherenceInfo>(source)));
    return ret;
}

static bool l1_is_hit(shared_ptr<cacheRequest> req, cacheLine& line, const uint64_t& system_time) { 
    shared_ptr<privateSharedLCC::coherenceInfo> info = 
        static_pointer_cast<privateSharedLCC::coherenceInfo>(line.coherence_info);
    mh_assert(req->request_type() == CACHE_REQ_READ || req->request_type() == CACHE_REQ_UPDATE);
    if (req->request_type() == CACHE_REQ_READ) {
        if (info->synched_expiration_time) {
            return system_time < *info->synched_expiration_time;
        } else {
            return system_time < info->expiration_time;
        }
    }
    return true;
}

static bool l2_is_hit(shared_ptr<cacheRequest> req, cacheLine &line, const uint64_t& system_time) {
    shared_ptr<privateSharedLCC::coherenceInfo> info =
        static_pointer_cast<privateSharedLCC::coherenceInfo>(line.coherence_info);
    if (req->request_type() == CACHE_REQ_WRITE && !info->synched_expiration_time) {
#ifdef PRIVATE_BYPASS
        if (info->directory->size() == 1 && info->directory->count(req->requesting_core()) > 0) {
            return true;
        }
#endif
        return system_time >= info->expiration_time;
    }
    return true;
}
    
static void cache_reserve_line(cacheLine &line) { 
    if (!line.coherence_info) {
        line.coherence_info = 
            shared_ptr<privateSharedLCC::coherenceInfo>(new privateSharedLCC::coherenceInfo);
    }
    return; 
}

static bool l2_can_evict_line_inclusive(cacheLine &line, const uint64_t& system_time) {
    shared_ptr<privateSharedLCC::coherenceInfo> info = 
        static_pointer_cast<privateSharedLCC::coherenceInfo>(line.coherence_info);
    if (!info->synched_expiration_time) {
        return system_time >= info->expiration_time;
    }
    return true;
}

static uint32_t replacement_policy_nearest_expiration_and_random(vector<uint32_t>& evictables, 
                                                                 cacheLine const* lines,
                                                                 const uint64_t& system_time,
                                                                 shared_ptr<random_gen> ran) {
    vector<uint32_t> nearest;
    uint64_t min_remaining = UINT64_MAX;
    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<privateSharedLCC::coherenceInfo> info = 
            static_pointer_cast<privateSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        uint64_t timestamp = info->synched_expiration_time ? *info->synched_expiration_time : info->expiration_time;
        uint64_t remaining = (system_time < timestamp) ? timestamp - system_time : 0;
        if (remaining < min_remaining) {
            nearest.clear();
            nearest.push_back(*i);
            min_remaining = remaining;
        } else if (remaining == min_remaining) {
            nearest.push_back(*i);
        }
    }
    return nearest[ran->random_range(nearest.size())];
}

static uint32_t replacement_policy_nearest_expiration_and_lru(vector<uint32_t>& evictables, 
                                                              cacheLine const* lines,
                                                              const uint64_t& system_time,
                                                              shared_ptr<random_gen> ran) {
    uint32_t evict_way = 0;
    uint64_t min_remaining = UINT64_MAX;
    uint64_t lru_time = UINT64_MAX;
    for (vector<uint32_t>::iterator i = evictables.begin(); i != evictables.end(); ++i) {
        shared_ptr<privateSharedLCC::coherenceInfo> info = 
            static_pointer_cast<privateSharedLCC::coherenceInfo>(lines[*i].coherence_info);
        uint64_t timestamp = info->synched_expiration_time ? *info->synched_expiration_time : info->expiration_time;
        uint64_t remaining = (system_time < timestamp) ? timestamp - system_time : 0;
        if (remaining < min_remaining) {
            evict_way = *i;
            lru_time = lines[*i].last_access_time;
            min_remaining = remaining;
        } else if (remaining == min_remaining) {
            if (lines[*i].last_access_time < lru_time) {
                lru_time = lines[*i].last_access_time;
                evict_way = *i;
            }
        }
    }
    return evict_way;
}

privateSharedLCC::privateSharedLCC(uint32_t id, 
                                   const uint64_t &t, 
                                   shared_ptr<tile_statistics> st, 
                                   logger &l, 
                                   shared_ptr<random_gen> r, 
                                   shared_ptr<cat> a_cat, 
                                   privateSharedLCCCfg_t cfg) :
    memory(id, t, st, l, r), 
    m_cfg(cfg), 
    m_l1(NULL), 
    m_l2(NULL), 
    m_cat(a_cat), 
    m_stats(shared_ptr<privateSharedLCCStatsPerTile>()),
    m_l2_work_table_vacancy_shared(cfg.l2_work_table_size_shared),
    m_l2_work_table_vacancy_readonly(cfg.l2_work_table_size_readonly),
    m_available_core_ports(cfg.num_local_core_ports)
{
    if (m_cfg.bytes_per_flit == 0) throw err_bad_shmem_cfg("flit size must be non-zero.");
    if (m_cfg.words_per_cache_line == 0) throw err_bad_shmem_cfg("cache line size must be non-zero.");
    if (m_cfg.lines_in_l1 == 0) throw err_bad_shmem_cfg("privateSharedLCC : L1 size must be non-zero.");
    if (m_cfg.lines_in_l2 == 0) throw err_bad_shmem_cfg("privateSharedLCC : L2 size must be non-zero.");
    if (m_cfg.l2_work_table_size_shared == 0) 
        throw err_bad_shmem_cfg("privateSharedLCC : shared L2 work table must be non-zero.");
    if (m_cfg.num_local_core_ports == 0) 
        throw err_bad_shmem_cfg("privateSharedLCC : The number of core ports must be non-zero.");

    replacementPolicy_t l1_policy, l2_policy;

    switch (cfg.l1_replacement_policy) {
    case _REPLACE_LRU:
        l1_policy = REPLACE_LRU;
        break;
    case _REPLACE_RANDOM:
        l1_policy = REPLACE_RANDOM;
        break;
    default:
        l1_policy = REPLACE_CUSTOM;
        break;
    }

    switch (cfg.l2_replacement_policy) {
    case _REPLACE_LRU:
        l2_policy = REPLACE_LRU;
        break;
    case _REPLACE_RANDOM:
        l2_policy = REPLACE_RANDOM;
        break;
    default:
        l2_policy = REPLACE_CUSTOM;
        break;
    }

    m_l1 = new cache(1, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l1, cfg.l1_associativity, l1_policy,
                     cfg.l1_hit_test_latency, cfg.l1_num_read_ports, cfg.l1_num_write_ports);
    m_l2 = new cache(2, id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l2, cfg.l2_associativity, l2_policy,
                     cfg.l2_hit_test_latency, cfg.l2_num_read_ports, cfg.l2_num_write_ports);

    m_l1->set_helper_copy_coherence_info(&copy_coherence_info);
    m_l1->set_helper_is_hit(&l1_is_hit);
    m_l1->set_helper_reserve_line(&cache_reserve_line);

    m_l2->set_helper_copy_coherence_info(&copy_coherence_info);
    m_l2->set_helper_is_hit(&l2_is_hit);
    m_l2->set_helper_reserve_line(&cache_reserve_line);

    switch (cfg.l1_replacement_policy) {
    case _REPLACE_NEAREST_EXPIRATION_AND_LRU:
        m_l1->set_helper_replacement_policy(&replacement_policy_nearest_expiration_and_lru);
        break;
    case _REPLACE_NEAREST_EXPIRATION_AND_RANDOM:
        m_l1->set_helper_replacement_policy(&replacement_policy_nearest_expiration_and_random);
        break;
    default:
        break;
    }

    if (!m_cfg.save_timestamp_in_dram) {
        m_l2->set_helper_can_evict_line(&l2_can_evict_line_inclusive);
    }

}

privateSharedLCC::~privateSharedLCC() {
    delete m_l1;
    delete m_l2;
}

uint32_t privateSharedLCC::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

uint64_t privateSharedLCC::get_expiration_time(shared_ptr<cacheLine> line) {
    if (line) {
        shared_ptr<coherenceInfo> info = static_pointer_cast<coherenceInfo>(line->coherence_info);
        if (info) {
            if (info->synched_expiration_time) {
                return *info->synched_expiration_time;
            } else {
                return info->expiration_time;
            }
        }
    }
    return 0;
}

void privateSharedLCC::request(shared_ptr<memoryRequest> req) {

    /* assumes a request is not across multiple cache lines */
    uint32_t __attribute__((unused)) byte_offset = req->maddr().address%(m_cfg.words_per_cache_line*4);
    mh_assert( (byte_offset + req->word_count()*4) <= m_cfg.words_per_cache_line * 4);

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    m_core_port_schedule_q.push_back(req);

}

void privateSharedLCC::tick_positive_edge() {
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
    if(m_dram_controller) {
        m_dram_controller->tick_positive_edge();
    }
}

void privateSharedLCC::tick_negative_edge() {

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

void privateSharedLCC::l1_work_table_update() {
    for (toL1Table::iterator it_addr = m_l1_work_table.begin(); it_addr != m_l1_work_table.end(); ) {

        maddr_t start_maddr = it_addr->first;
        shared_ptr<toL1Entry> entry = it_addr->second;

        shared_ptr<memoryRequest> core_req = entry->core_req;
        shared_ptr<cacheRequest> l1_req = entry->l1_req;
        shared_ptr<catRequest> cat_req = entry->cat_req;
        shared_ptr<coherenceMsg> lcc_req = entry->lcc_req;
        shared_ptr<coherenceMsg> lcc_rep = entry->lcc_rep; 
        shared_ptr<message_t> msg_to_send = entry->net_msg_to_send;

        shared_ptr<cacheLine> line = (l1_req)? l1_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> victim = (l1_req)? l1_req->victim_line_copy() : shared_ptr<cacheLine>();

        if (entry->status == _L1_WORK_WAIT_L1) {
            if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

#ifdef PRIVATE_BYPASS
            if (!core_req->is_read()) {

                if (cat_req->status() == CAT_REQ_DONE) {
                    uint32_t dir_home = cat_req->home();
                    if (stats_enabled()) {
                        stats()->did_read_cat(dir_home == m_id);
                    }
                    lcc_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                    lcc_req->sender = m_id;
                    lcc_req->receiver = dir_home;
                    lcc_req->did_win_last_arbitration = false;
                    lcc_req->waited = 0;
                    lcc_req->type = WRITE_REQ;
                    lcc_req->word_count = core_req->word_count();
                    lcc_req->maddr = core_req->maddr();
                    lcc_req->data = core_req->data();

                    /* cost breakdown study */
                    lcc_req->milestone_time = system_time;

                    entry->lcc_req = lcc_req;
                    if (dir_home == m_id) {
                        m_to_l2_req_schedule_q.push_back(lcc_req);
                    } else {
                        msg_to_send = shared_ptr<message_t>(new message_t);
                        msg_to_send->src = m_id;
                        msg_to_send->dst = dir_home;
                        if (m_cfg.use_separate_vc_for_writes && lcc_req->type == WRITE_REQ) {
                            msg_to_send->type = MSG_LCC_REQ_2;
                        } else {
                            msg_to_send->type = MSG_LCC_REQ_1;
                        }
                        uint32_t data_size = core_req->word_count() * 4;
                        msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                        msg_to_send->content = lcc_req;
                        entry->net_msg_to_send = msg_to_send;
                        m_to_network_schedule_q[msg_to_send->type].push_back(msg_to_send);
                    }
                    entry->status = _L1_WORK_SEND_LCC_REQ;
                } else {
                    entry->status = _L1_WORK_WAIT_CAT;
                    cat_req->set_milestone_time(system_time);
                }

                /* cost breakdown study */
                entry->milestone_time = system_time;
                if (stats_enabled() && l1_req->milestone_time() != UINT64_MAX) {
                    stats()->add_l1_action_cost(system_time - l1_req->milestone_time());
                }
                ++it_addr;
                continue;
            }
#endif

            if (l1_req->status() == CACHE_REQ_HIT) {
                shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                    ret[i] = line->data[i + word_offset];
                }
                set_req_data(core_req, ret);
                set_req_status(core_req, REQ_DONE);
                ++m_available_core_ports;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a HIT and finish serving address "
                          << core_req->maddr() << endl;
                if (stats_enabled()) {
                    stats()->did_read_l1(true);
                    stats()->did_finish_read(system_time - entry->requested_time);

                    /* cost breakdown study */
                    if (l1_req->milestone_time() != UINT64_MAX) {
                        stats()->add_l1_action_cost(system_time - l1_req->milestone_time());
                    }

                }
                m_l1_work_table.erase(it_addr++);
                continue;
            } else {
                /* read miss */
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] gets a MISS for address " << core_req->maddr() << endl;

                /* cost breakdown study */
                if (stats_enabled() && l1_req->milestone_time() != UINT64_MAX) {
                    stats()->add_l1_action_cost(system_time - l1_req->milestone_time());
                }
                entry->milestone_time = system_time;

                if (cat_req->status() == CAT_REQ_DONE) {
                    uint32_t dir_home = cat_req->home();
                    if (stats_enabled()) {
                        stats()->did_read_cat(dir_home == m_id);
                    }
                    lcc_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                    lcc_req->sender = m_id;
                    lcc_req->receiver = dir_home;
                    lcc_req->type = core_req->is_read() ? READ_REQ : WRITE_REQ;
                    lcc_req->word_count = core_req->is_read() ? m_cfg.words_per_cache_line : core_req->word_count();
                    lcc_req->maddr = start_maddr;
                    lcc_req->data = core_req->is_read() ? shared_array<uint32_t>() : core_req->data();
                    lcc_req->did_win_last_arbitration = false;
                    lcc_req->waited = 0;

                    /* cost breakdown study */
                    lcc_req->milestone_time = system_time;

                    entry->lcc_req = lcc_req;
                    if (dir_home == m_id) {
                        m_to_l2_req_schedule_q.push_back(lcc_req);
                    } else {
                        msg_to_send = shared_ptr<message_t>(new message_t);
                        msg_to_send->src = m_id;
                        msg_to_send->dst = dir_home;
                        if (m_cfg.use_separate_vc_for_writes && lcc_req->type == WRITE_REQ) {
                            msg_to_send->type = MSG_LCC_REQ_2;
                        } else {
                            msg_to_send->type = MSG_LCC_REQ_1;
                        }
                        uint32_t data_size = (lcc_req->type == WRITE_REQ) ? core_req->word_count() * 4 : 0;
                        msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                        msg_to_send->content = lcc_req;
                        entry->net_msg_to_send = msg_to_send;
                        m_to_network_schedule_q[msg_to_send->type].push_back(msg_to_send);
                    }
                    entry->status = _L1_WORK_SEND_LCC_REQ;
                } else {
                    entry->status = _L1_WORK_WAIT_CAT;
                }
                ++it_addr;
                continue;
            }
        } else if (entry->status == _L1_WORK_WAIT_CAT) {
            if (cat_req->status() == CAT_REQ_DONE) {
                uint32_t dir_home = cat_req->home();
                if (stats_enabled()) {
                    stats()->did_read_cat(dir_home == m_id);
                }

                /* csst breakdown study */
                if (stats_enabled() && cat_req->milestone_time() != UINT64_MAX) {
                    stats()->add_cat_action_cost(system_time - cat_req->milestone_time());
                }
                entry->milestone_time = system_time;

                lcc_req = shared_ptr<coherenceMsg>(new coherenceMsg);
                lcc_req->sender = m_id;
                lcc_req->receiver = dir_home;

                /* cost breakdown study */
                lcc_req->milestone_time = system_time;

                if (core_req->is_read()) {
                    lcc_req->type = READ_REQ;
                    lcc_req->word_count = m_cfg.words_per_cache_line;
                    lcc_req->maddr = start_maddr;
                    lcc_req->data = shared_array<uint32_t>();
                } else {
                    lcc_req->type = WRITE_REQ;
                    lcc_req->word_count = core_req->word_count();
                    lcc_req->maddr = core_req->maddr();
                    lcc_req->data = core_req->data();
                }
                lcc_req->did_win_last_arbitration = false;
                lcc_req->waited = 0;
                lcc_req->timestamp = 0; /* don't care */
                entry->lcc_req = lcc_req;
                if (dir_home == m_id) {
                    m_to_l2_req_schedule_q.push_back(lcc_req);
                } else {
                    msg_to_send = shared_ptr<message_t>(new message_t);
                    msg_to_send->src = m_id;
                    msg_to_send->dst = dir_home;
                    if (m_cfg.use_separate_vc_for_writes && lcc_req->type == WRITE_REQ) {
                        msg_to_send->type = MSG_LCC_REQ_2;
                    } else {
                        msg_to_send->type = MSG_LCC_REQ_1;
                    }
                    uint32_t data_size = (lcc_req->type == WRITE_REQ) ? core_req->word_count() * 4 : 0;
                    msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                    msg_to_send->content = lcc_req;
                    entry->net_msg_to_send = msg_to_send;
                    m_to_network_schedule_q[msg_to_send->type].push_back(msg_to_send);
                }
                entry->status = _L1_WORK_SEND_LCC_REQ;
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L1_WORK_SEND_LCC_REQ) {
            if (lcc_req->did_win_last_arbitration) {
                lcc_req->did_win_last_arbitration = false;
                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                entry->status = _L1_WORK_WAIT_LCC_REP;
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] sent an LCC request (" << lcc_req->type
                          << ") to " << lcc_req->receiver << " for " << lcc_req->maddr << endl;
            } else {
                if (lcc_req->receiver == m_id) {
                    m_to_l2_req_schedule_q.push_back(lcc_req);
                } else {
                    m_to_network_schedule_q[msg_to_send->type].push_back(msg_to_send);
                }
            }
            ++it_addr;
            continue;
        } else if (entry->status == _L1_WORK_WAIT_LCC_REP) {
            if (lcc_rep) {
                mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received an LCC reply (" << lcc_rep->type
                          << ") from " << lcc_rep->sender << " for " << lcc_rep->maddr << endl;

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - lcc_rep->milestone_time);
                }

                if (lcc_req->type == WRITE_REQ) {
                    /* not necessary, but return what has been written */
                    shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                    for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                        ret[i] = lcc_rep->data[i];
                    }
                    set_req_data(core_req, ret);
                    set_req_status(core_req, REQ_DONE);
                    ++m_available_core_ports;
                    mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] finish writing address "
                              << core_req->maddr() << " through " << lcc_rep->sender << endl;
                    m_l1_work_table.erase(it_addr++);
                    if (stats_enabled()) {
                        stats()->did_finish_write(system_time - entry->requested_time);
                    }
                    continue;
                } else {
                    uint64_t timestamp = lcc_rep->synched_timestamp? *lcc_rep->synched_timestamp : lcc_rep->timestamp;
                    if (system_time < timestamp) {
                        shared_ptr<coherenceInfo> line_info(new coherenceInfo);
                        line_info->expiration_time = lcc_rep->timestamp;
                        line_info->synched_expiration_time = lcc_rep->synched_timestamp;
                        l1_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE, 
                                                                           m_cfg.words_per_cache_line,
                                                                           lcc_rep->data, line_info));
                        l1_req->set_clean_write(true);
                        l1_req->set_reserve(true);
                        entry->l1_req = l1_req;
                        entry->status = _L1_WORK_UPDATE_L1;

                        /* cost breakdown study */
                        entry->milestone_time = system_time;
                        l1_req->set_milestone_time(system_time);

                    } else {
                        shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
                        uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
                        for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                            ret[i] = lcc_rep->data[i + word_offset];
                        }
                        set_req_data(core_req, ret);
                        set_req_status(core_req, REQ_DONE);
                        ++m_available_core_ports;
                        mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] resolved a MISS and finish serving address "
                            << core_req->maddr() << " without caching in L1 " << endl;
                        if (stats_enabled()) {
                            stats()->did_read_l1(false);
                            stats()->did_finish_read(system_time - entry->requested_time);
                        }
                        m_l1_work_table.erase(it_addr++);
                        continue;
                    }
                }
            } 
            ++it_addr;
            continue;
        } else if (entry->status == _L1_WORK_UPDATE_L1) {
            if (l1_req->status() == CACHE_REQ_NEW || l1_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            if (stats_enabled() && l1_req->milestone_time() != UINT64_MAX) {
                stats()->add_l1_action_cost(system_time - l1_req->milestone_time());
            }
            entry->milestone_time = system_time;

            if (l1_req->status() == CACHE_REQ_MISS) {
                /* evicted a line and reserved its space */
                l1_req->reset();
                l1_req->set_milestone_time(system_time);
                ++it_addr;
                continue;
            }
            shared_array<uint32_t> ret(new uint32_t[core_req->word_count()]);
            uint32_t word_offset = (core_req->maddr().address / 4 ) % m_cfg.words_per_cache_line;
            for (uint32_t i = 0; i < core_req->word_count(); ++i) {
                ret[i] = line->data[i + word_offset];
            }
            set_req_data(core_req, ret);
            set_req_status(core_req, REQ_DONE);
            ++m_available_core_ports;
            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] resolved a MISS and finish serving address "
                << core_req->maddr() << endl;
            if (stats_enabled()) {
                stats()->did_read_l1(false);
                stats()->did_finish_read(system_time - entry->requested_time);
            }
            m_l1_work_table.erase(it_addr++);
            continue;
        }
    }
}

void privateSharedLCC::l2_work_table_update() {
    for (toL2Table::iterator it_addr = m_l2_work_table.begin(); it_addr != m_l2_work_table.end(); ) {
        maddr_t start_maddr = it_addr->first;
        shared_ptr<toL2Entry> entry = it_addr->second;

        shared_ptr<cacheRequest> l2_req = entry->l2_req;
        shared_ptr<cacheLine> line = l2_req ? l2_req->line_copy() : shared_ptr<cacheLine>();
        shared_ptr<cacheLine> victim = l2_req ? l2_req->victim_line_copy() : shared_ptr<cacheLine>();
        shared_ptr<coherenceInfo> line_info = 
            line ? static_pointer_cast<coherenceInfo>(line->coherence_info) : shared_ptr<coherenceInfo>();
        shared_ptr<coherenceInfo> victim_info = 
            victim ? static_pointer_cast<coherenceInfo>(victim->coherence_info) : shared_ptr<coherenceInfo>();

        shared_ptr<coherenceMsg> lcc_read_req = entry->lcc_read_req;
        shared_ptr<coherenceMsg> lcc_write_req = entry->lcc_write_req;

        shared_ptr<coherenceMsg> lcc_rep = entry->lcc_rep;
        shared_ptr<dramMsg> dram_req = entry->dram_req;
        shared_ptr<dramMsg> dram_rep = entry->dram_rep;
        shared_ptr<message_t> msg_to_send = entry->net_msg_to_send;

        if (entry->status == _L2_WORK_WAIT_L2) {
            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            }

            /* cost breakdown study */
            if (stats_enabled() && l2_req->milestone_time() != UINT64_MAX) {
                stats()->add_l2_action_cost(system_time - l2_req->milestone_time());
            }

            if (l2_req->status() == CACHE_REQ_HIT) {
                /* HIT */
                entry->waiting_for_evictions = false;

                mh_assert(line_info);
                if (lcc_write_req) {
                    /* WRITE */
                    if (m_cfg.logic == TIMESTAMP_IDEAL) {
#ifdef PRIVATE_BYPASS
                        if (*line_info->synched_expiration_time > system_time &&
                            (line_info->directory->size() > 1 || line_info->directory->count(lcc_write_req->sender) == 0)
                           ) {
#else
                        if (*line_info->synched_expiration_time > system_time) {
#endif
                            *line_info->synched_expiration_time = system_time;

                            mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] has invalidated all shared lines for "
                                      << start_maddr << " due to a write." << " firt read time since last expiration : " 
                                      << hex << *line_info->first_read_time_in_phase << dec << endl;

#ifdef PRINT_IDEAL
                            if (*line_info->first_read_time_in_phase != UINT64_MAX) {
                                cout << "(w) " << start_maddr << " @ " << system_time << " " 
                                    << *line_info->last_read_time - *line_info->first_read_time_in_phase << " "
                                    << system_time - *line_info->first_read_time_in_phase << " "
                                    << *line_info->num_reads_in_phase << " "
                                    << line_info->directory->size() << " "
                                    << endl;
                            }
#endif
                        }
                    }

#ifdef PRIVATE_BYPASS
                    if (line_info->expiration_time > system_time) {
                        *line_info->first_read_time_in_phase = system_time;
                        *line_info->last_write_time = system_time;
                        *line_info->num_reads_in_phase = 1;
                    } else {
                        *line_info->first_read_time_in_phase = UINT64_MAX;
                        *line_info->last_write_time = system_time;
                        *line_info->num_reads_in_phase = 0;
                        line_info->directory->clear();
                    }
#else
                    *line_info->first_read_time_in_phase = UINT64_MAX;
                    *line_info->last_write_time = system_time;
                    *line_info->num_reads_in_phase = 0;
                    line_info->directory->clear();
#endif

                } else {
                    /* READ */
                    bool need_to_update_l2 = false;

                    if (m_cfg.logic == TIMESTAMP_IDEAL) {

                        if (*line_info->synched_expiration_time < system_time) {
#ifdef MAX_IDEAL
                            line_info->synched_expiration_time = shared_ptr<uint64_t>(new uint64_t(system_time + MAX_IDEAL));
#else
                            line_info->synched_expiration_time = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
#endif
                            line_info->directory->clear();
                            *line_info->num_reads_in_phase = 0;
                            *line_info->first_read_time_in_phase = UINT64_MAX;
#ifdef PRINT_IDEAL
                            cout << "(f) " << start_maddr << " @ " << system_time << " " 
                                 << line_info->directory->size() << " "
                                 << endl;
#endif
                        }
                    } else if (m_cfg.logic == TIMESTAMP_FIXED) {
                        if (!entry->write_requests_waiting) {
                            if (line_info->expiration_time < system_time) {
                                line_info->directory->clear();
                                *line_info->num_reads_in_phase = 0;
                                *line_info->first_read_time_in_phase = UINT64_MAX;
                            }
                            line_info->expiration_time = system_time + m_cfg.default_timestamp_delta;
                            /* C# 1 */
                            need_to_update_l2 = true;
                        }
                    } else if (m_cfg.logic == TIMESTAMP_ZERO_DELAY) {
                        /* TODO : SMART */
                        /* read hit */
                        if (!entry->write_requests_waiting) {
                            uint32_t delta = 0;
                            uint64_t v = system_time - *line_info->last_write_time;

                            int l2_x = (int)m_id % 8;
                            int l2_y = (int)m_id / 8;
                            int l1_x = (int)lcc_read_req->sender % 8;
                            int l1_y = (int)lcc_read_req->sender / 8;
                            int dist = abs(l2_x - l1_x) + abs(l1_y - l2_y);
                            uint32_t trip = 8 + dist + get_flit_count(1 + (m_cfg.words_per_cache_line + TIMESTAMP_WORDS) * 4);

                            if (v > W2) {
                                delta = MR + trip;
                            } else if (v < W1) {
                                delta = LR + trip;
                            } else {
#if 0
                                double W_to_R = 2.0;
                                int l2_x = (int)m_id % 8;
                                int l2_y = (int)m_id / 8;
                                int l1_x = (int)lcc_read_req->sender % 8;
                                int l1_y = (int)lcc_read_req->sender / 8;
                                int dist = abs(l2_x - l1_x) + abs(l1_y - l2_y);
                                uint32_t cost = 2*(dist + get_flit_count(1 + (m_cfg.words_per_cache_line + TIMESTAMP_WORDS) * 4));
                                delta = cost / W_to_R;
                                _d = delta;
#endif
                                delta = LR + (MR-LR)*(v-W1)/(W2-W1) + trip;
                            }

                            if (system_time + delta > line_info->expiration_time) {
                            //    cout << "dist " << v << " delta " << delta << endl;
                                line_info->expiration_time = system_time + delta;
                                need_to_update_l2 = true;
                            }
                        }

                    }

                    line_info->directory->insert(lcc_read_req->sender);
                    *line_info->num_reads_in_phase += 1;
                    *line_info->last_read_time = system_time;
                    if (*line_info->first_read_time_in_phase == UINT64_MAX) {
                        *line_info->first_read_time_in_phase = system_time;
                    }

                    if (need_to_update_l2) {
                        mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] renews timestamp for "
                            << start_maddr << " to " << line_info->expiration_time 
                            << " due to a read on an expired line." << endl;
                        l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                           m_cfg.words_per_cache_line,
                                                                           line->data, line_info));
                        l2_req->set_reserve(true);
                        l2_req->set_clean_write(true);
                        entry->l2_req = l2_req;

                        /* cost breakdown study */
                        l2_req->set_milestone_time(system_time);

                        entry->status = _L2_WORK_UPDATE_L2;

                        /* an LCC reply will be sent in _L2_WORK_UPDATE_L2 state */

                        ++it_addr;
                        continue;
                    }
                }

                lcc_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                lcc_rep->sender = m_id;
                lcc_rep->did_win_last_arbitration = false;
                lcc_rep->waited = 0;
                if (lcc_write_req) {
                    lcc_rep->type = WRITE_REP;
                    lcc_rep->receiver = lcc_write_req->sender;
                    lcc_rep->word_count = lcc_write_req->word_count;
                    lcc_rep->maddr = lcc_write_req->maddr;
                    lcc_rep->data = lcc_write_req->data;
                    /* not used */
                    lcc_rep->timestamp = get_expiration_time(line);
                    lcc_rep->synched_timestamp = line_info->synched_expiration_time;
                } else {
                    lcc_rep->type = READ_REP;
                    lcc_rep->receiver = lcc_read_req->sender;
                    lcc_rep->word_count = m_cfg.words_per_cache_line;
                    lcc_rep->maddr = start_maddr;
                    lcc_rep->data = line->data;
                    lcc_rep->timestamp = get_expiration_time(line);
                    lcc_rep->synched_timestamp = line_info->synched_expiration_time;
                }
                entry->lcc_rep = lcc_rep;

                /* cost breakdown study */
                lcc_rep->milestone_time = system_time;

                if (lcc_rep->receiver == m_id) {
                    mh_assert(m_l1_work_table.count(start_maddr) &&
                              m_l1_work_table[start_maddr]->status == _L1_WORK_WAIT_LCC_REP);
                    m_l1_work_table[start_maddr]->lcc_rep = lcc_rep;
                    lcc_rep->did_win_last_arbitration = true;
                    if (lcc_rep->type == READ_REP) {
                        mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent an LCC read reply for " 
                                  << start_maddr << " to " << m_id << " timestamp ";
                        if (lcc_rep->synched_timestamp) {
                            mh_log(4) << *lcc_rep->synched_timestamp << endl;
                        } else {
                            mh_log(4) << lcc_rep->timestamp << endl;
                        }
                        if (stats_enabled()) {
                            stats()->did_read_l2(true);
                        }
                    } else {
                        mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent an LCC write reply for " 
                                  << lcc_write_req->maddr << " to " << m_id << endl;
                        if (stats_enabled()) {
                            stats()->did_write_l2(true);
                        }
                    }
                    if (entry->using_space_for_reads) {
                        ++m_l2_work_table_vacancy_readonly;
                    } else {
                        ++m_l2_work_table_vacancy_shared;
                    }
                    m_l2_work_table.erase(it_addr++);
                    continue;
                } else {
                    msg_to_send = shared_ptr<message_t>(new message_t);
                    msg_to_send->type = MSG_LCC_REP;
                    msg_to_send->src = m_id;
                    msg_to_send->dst = lcc_rep->receiver;
                    if (lcc_read_req) {
                        msg_to_send->flit_count = get_flit_count(1 + (m_cfg.words_per_cache_line + TIMESTAMP_WORDS) * 4);
                    } else {
                        msg_to_send->flit_count = get_flit_count(1);
                    }
                    msg_to_send->content = lcc_rep;
                    entry->net_msg_to_send = msg_to_send;
                    m_to_network_schedule_q[MSG_LCC_REP].push_back(msg_to_send);
                    entry->status = _L2_WORK_SEND_LCC_REP_THEN_FINISH;
                    ++it_addr;
                    continue;
                }

            } else {
                /* MISS */

                entry->did_miss_on_first = true;

                /* cost breakdown study */
                entry->milestone_time = system_time;

                if (line) {
                    entry->waiting_for_evictions = false;
                }

                if (!line) {
                    /* could not evict */

                    if (!entry->waiting_for_evictions) {
                        entry->waiting_for_evictions = true;
                        if (stats_enabled()) {
                            stats()->evict_blocked_by_timestamp();
                        }
                    }
                    /* keep retrying */
                    l2_req->reset();
                    l2_req->set_milestone_time(system_time);

                } else if (line->valid) {
                    /* unexpired for a write */

                    mh_assert(m_cfg.logic != TIMESTAMP_IDEAL);
                    mh_assert(line_info->expiration_time > system_time);
                    entry->accept_read_requests = true;
                    entry->status = _L2_WORK_WAIT_TIMESTAMP;

                    /* cost breakdown study */
                    if (stats_enabled()) {
                        stats()->add_l2_write_block_cost(line_info->expiration_time - system_time);
                        stats()->write_blocked_by_timestamp();
                    }

                    entry->milestone_time = line_info->expiration_time;

                    mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] blocks a write request for " << start_maddr
                              << " from " << lcc_write_req->sender << " (expiration time : " << line_info->expiration_time 
                              << " ) " << endl;

                } else if (victim) {
                    /* there is a victim */

                    if (m_cfg.logic == TIMESTAMP_IDEAL && !m_cfg.save_timestamp_in_dram) {
                        if (*victim_info->synched_expiration_time > system_time) {
#ifdef PRINT_IDEAL
                            cout << "(e) " << start_maddr << " @ " << system_time << " " 
                                 << *victim_info->last_read_time - *victim_info->first_read_time_in_phase << " "
                                 << system_time - *victim_info->first_read_time_in_phase << " "
                                 << *victim_info->num_reads_in_phase << " "
                                 << victim_info->directory->size() 
                                 << endl;
#endif
                            *victim_info->synched_expiration_time = system_time;

                            mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] has invalidated all shared lines for "
                                << victim->start_maddr << " due to an eviction." << " first read time since last expiration : " 
                                << hex << *victim_info->first_read_time_in_phase << dec << endl;
                        }
                        *victim_info->first_read_time_in_phase = UINT64_MAX;
                        victim_info->directory->clear();
                        *victim_info->num_reads_in_phase = 0;
                    }

                    if (victim->dirty || m_cfg.save_timestamp_in_dram) {
                        /* DRAM writeback for the victim */

                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        uint32_t data_size = 0;
                        if (m_cfg.save_timestamp_in_dram) {
                            dram_req->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                    DRAM_REQ_WRITE, 
                                                                                    m_cfg.words_per_cache_line, 
                                                                                    victim->data,
                                                                                    TIMESTAMP_WORDS,
                                                                                    victim_info));
                            data_size = (m_cfg.words_per_cache_line + TIMESTAMP_WORDS) * 4;
                        } else {
                            dram_req->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                    DRAM_REQ_WRITE, 
                                                                                    m_cfg.words_per_cache_line, 
                                                                                    victim->data));
                            data_size = m_cfg.words_per_cache_line * 4;
                        }
                        dram_req->did_win_last_arbitration = false;
                        entry->dram_req = dram_req;
                        if (m_dram_controller_location == m_id) {
                            m_to_dram_req_schedule_q.push_back(dram_req);
                        } else {
                            msg_to_send = shared_ptr<message_t>(new message_t);
                            msg_to_send->src = m_id;
                            msg_to_send->dst = m_dram_controller_location;
                            msg_to_send->type = MSG_DRAM_REQ;
                            msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                            msg_to_send->content = dram_req;
                            entry->net_msg_to_send = msg_to_send;
                            m_to_network_schedule_q[MSG_DRAM_REQ].push_back(msg_to_send);
                        }

                        /* cost breakdown study */
                        dram_req->milestone_time = system_time;
                        entry->milestone_time = system_time;

                        entry->status = _L2_WORK_SEND_DRAM_WRITEBACK_THEN_FEED;

                    } else {
                        /* no need victim writeback */

                        dram_req = shared_ptr<dramMsg>(new dramMsg);
                        dram_req->sender = m_id;
                        dram_req->receiver = m_dram_controller_location;
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                                DRAM_REQ_READ, 
                                                                                m_cfg.words_per_cache_line)); 
                        dram_req->did_win_last_arbitration = false;
                        entry->dram_req = dram_req;
                        dram_req->milestone_time = system_time;
                        if (m_dram_controller_location == m_id) {
                            m_to_dram_req_schedule_q.push_back(dram_req);
                        } else {
                            msg_to_send = shared_ptr<message_t>(new message_t);
                            msg_to_send->src = m_id;
                            msg_to_send->dst = m_dram_controller_location;
                            msg_to_send->type = MSG_DRAM_REQ;
                            msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                            msg_to_send->content = dram_req;
                            entry->net_msg_to_send = msg_to_send;
                            m_to_network_schedule_q[MSG_DRAM_REQ].push_back(msg_to_send);
                        }
                        entry->status = _L2_WORK_SEND_DRAM_REQ;
                    }

                } else {
                    /* there is no victim */

                    dram_req = shared_ptr<dramMsg>(new dramMsg);
                    dram_req->sender = m_id;
                    dram_req->receiver = m_dram_controller_location;
                    dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                            DRAM_REQ_READ, 
                                                                            m_cfg.words_per_cache_line)); 
                    dram_req->did_win_last_arbitration = false;
                    entry->dram_req = dram_req;
                    if (m_dram_controller_location == m_id) {
                        m_to_dram_req_schedule_q.push_back(dram_req);
                    } else {
                        msg_to_send = shared_ptr<message_t>(new message_t);
                        msg_to_send->src = m_id;
                        msg_to_send->dst = m_dram_controller_location;
                        msg_to_send->type = MSG_DRAM_REQ;
                        msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                        msg_to_send->content = dram_req;
                        entry->net_msg_to_send = msg_to_send;
                        m_to_network_schedule_q[MSG_DRAM_REQ].push_back(msg_to_send);
                    }

                    /* cost breakdown study */
                    dram_req->milestone_time = system_time;

                    entry->status = _L2_WORK_SEND_DRAM_REQ;
                }
                ++it_addr;
                continue;
            }

        } else if (entry->status == _L2_WORK_UPDATE_L2) {

            if (l2_req->status() == CACHE_REQ_NEW || l2_req->status() == CACHE_REQ_WAIT) {
                ++it_addr;
                continue;
            } 

            /* cost breakdown study */
            if (stats_enabled() && l2_req->milestone_time() != UINT64_MAX) {
                stats()->add_l2_action_cost(system_time - l2_req->milestone_time());
            }

            if (l2_req->status() == CACHE_REQ_MISS) {
                /* MISS */

                if (!line) {
                    /* could not evict */

                    if (!entry->waiting_for_evictions) {
                        entry->waiting_for_evictions = true;
                        if (stats_enabled()) {
                            stats()->evict_blocked_by_timestamp();
                        }
                    }

                    /* keep retrying */
                    l2_req->reset();
                    l2_req->set_milestone_time(system_time);
                    
                    ++it_addr;
                    continue;

                } else if (victim && (victim->dirty || m_cfg.save_timestamp_in_dram)) {
                    dram_req = shared_ptr<dramMsg>(new dramMsg);
                    dram_req->sender = m_id;
                    dram_req->receiver = m_dram_controller_location;
                    uint32_t data_size = 0;
                    if (m_cfg.save_timestamp_in_dram) {
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                DRAM_REQ_WRITE, 
                                                                                m_cfg.words_per_cache_line, 
                                                                                victim->data,
                                                                                TIMESTAMP_WORDS,
                                                                                victim_info));
                        data_size = (m_cfg.words_per_cache_line + TIMESTAMP_WORDS) * 4;
                    } else {
                        dram_req->req = shared_ptr<dramRequest>(new dramRequest(victim->start_maddr,
                                                                                DRAM_REQ_WRITE, 
                                                                                m_cfg.words_per_cache_line, 
                                                                                victim->data));
                        data_size = m_cfg.words_per_cache_line * 4;
                    }
                    dram_req->did_win_last_arbitration = false;
                    entry->dram_req = dram_req;
                    if (m_dram_controller_location == m_id) {
                        m_to_dram_req_schedule_q.push_back(dram_req);
                    } else {
                        msg_to_send = shared_ptr<message_t>(new message_t);
                        msg_to_send->src = m_id;
                        msg_to_send->dst = m_dram_controller_location;
                        msg_to_send->type = MSG_DRAM_REQ;
                        msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes + data_size);
                        msg_to_send->content = dram_req;
                        entry->net_msg_to_send = msg_to_send;
                        m_to_network_schedule_q[MSG_DRAM_REQ].push_back(msg_to_send);
                    }

                    /* cost breakdown study */
                    dram_req->milestone_time = system_time;
                    entry->milestone_time = system_time;

                    entry->status = _L2_WORK_SEND_DRAM_WRITEBACK_THEN_RETRY;

                } else {
                    /* evicted somebody and reserved its space this time. retry and will hit */

                    l2_req->reset();
                    l2_req->set_milestone_time(system_time);

                    ++it_addr;
                    continue;
                }

            } else {
                /* HIT */
                entry->waiting_for_evictions = false;

                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] updated cache line for " << start_maddr << endl;
                lcc_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                lcc_rep->sender = m_id;
                lcc_rep->did_win_last_arbitration = false;
                lcc_rep->waited = 0;
                if (lcc_write_req) {
                    lcc_rep->receiver = lcc_write_req->sender;
                    lcc_rep->type = WRITE_REP;
                    lcc_rep->word_count = lcc_write_req->word_count;
                    lcc_rep->maddr = lcc_write_req->maddr;
                    lcc_rep->data = lcc_write_req->data;
                    /* not used*/
                    lcc_rep->timestamp = get_expiration_time(line);
                    lcc_rep->synched_timestamp = line_info->synched_expiration_time;
                } else {
                    lcc_rep->receiver = lcc_read_req->sender;
                    lcc_rep->type = READ_REP;
                    lcc_rep->word_count = m_cfg.words_per_cache_line;
                    lcc_rep->maddr = start_maddr;
                    lcc_rep->data = line->data;
                    lcc_rep->timestamp = get_expiration_time(line);
                    lcc_rep->synched_timestamp = line_info->synched_expiration_time;
                }

                /* cost breakdown study */
                lcc_rep->milestone_time = system_time;
                
                entry->lcc_rep = lcc_rep;

                if (lcc_rep->receiver == m_id) {
                    mh_assert(m_l1_work_table.count(start_maddr) &&
                              m_l1_work_table[start_maddr]->status == _L1_WORK_WAIT_LCC_REP);
                    m_l1_work_table[start_maddr]->lcc_rep = lcc_rep;
                    lcc_rep->did_win_last_arbitration = true;
                    if (lcc_rep->type == READ_REP) {
                        mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent an LCC read reply for " 
                                  << start_maddr << " to " << m_id << " timestamp " ;
                        if (lcc_rep->synched_timestamp) {
                            mh_log(4) << *lcc_rep->synched_timestamp << endl;
                        } else {
                            mh_log(4) << lcc_rep->timestamp << endl;
                        }
                        if (stats_enabled()) {
                            stats()->did_read_l2(!entry->did_miss_on_first);
                        }
                    } else {
                        mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent an LCC write reply for " 
                                  << lcc_write_req->maddr << " to " << m_id << endl;
                        if (stats_enabled()) {
                            stats()->did_write_l2(!entry->did_miss_on_first);
                        }
                    }
                    if (entry->using_space_for_reads) {
                        ++m_l2_work_table_vacancy_readonly;
                    } else {
                        ++m_l2_work_table_vacancy_shared;
                    }
                    m_l2_work_table.erase(it_addr++);
                    continue;
                } else {
                    msg_to_send = shared_ptr<message_t>(new message_t);
                    msg_to_send->type = MSG_LCC_REP;
                    msg_to_send->src = m_id;
                    msg_to_send->dst = lcc_rep->receiver;
                    if (lcc_rep->type == READ_REP) {
                        msg_to_send->flit_count = get_flit_count(1 + (TIMESTAMP_WORDS + m_cfg.words_per_cache_line) * 4);
                    } else {
                        msg_to_send->flit_count = get_flit_count(1);
                    }
                    msg_to_send->content = lcc_rep;
                    entry->net_msg_to_send = msg_to_send;
                    m_to_network_schedule_q[MSG_LCC_REP].push_back(msg_to_send);
                    entry->status = _L2_WORK_SEND_LCC_REP_THEN_FINISH;
                    ++it_addr;
                    continue;
                }
            }

        } else if (entry->status == _L2_WORK_SEND_DRAM_WRITEBACK_THEN_FEED) {
            if (dram_req->did_win_last_arbitration) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_network_plus_serialization_cost(system_time - entry->milestone_time);
                }

                dram_req->did_win_last_arbitration = false;
                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                dram_req = shared_ptr<dramMsg>(new dramMsg);
                dram_req->sender = m_id;
                dram_req->receiver = m_dram_controller_location;
                dram_req->req = shared_ptr<dramRequest>(new dramRequest(start_maddr,
                                                                        DRAM_REQ_READ, 
                                                                        m_cfg.words_per_cache_line)); 
                entry->dram_req = dram_req;

                /* cost breakdown study */
                dram_req->milestone_time = system_time;
                entry->milestone_time = system_time;

                if (m_dram_controller_location == m_id) {
                    m_to_dram_req_schedule_q.push_back(dram_req);
                } else {
                    msg_to_send = shared_ptr<message_t>(new message_t);
                    msg_to_send->src = m_id;
                    msg_to_send->dst = m_dram_controller_location;
                    msg_to_send->type = MSG_DRAM_REQ;
                    msg_to_send->flit_count = get_flit_count(1 + m_cfg.address_size_in_bytes);
                    msg_to_send->content = dram_req;
                    entry->net_msg_to_send = msg_to_send;
                    m_to_network_schedule_q[MSG_DRAM_REQ].push_back(msg_to_send);
                }
                entry->status = _L2_WORK_SEND_DRAM_REQ;
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_SEND_DRAM_WRITEBACK_THEN_RETRY) {
            if (dram_req->did_win_last_arbitration) {

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_network_plus_serialization_cost(system_time - dram_req->milestone_time);
                }

                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent a dram request for writing back for "
                          << dram_req->req->maddr() << endl;
                dram_req->did_win_last_arbitration = false;
                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                l2_req->reset();
                l2_req->set_milestone_time(system_time);

                entry->status = _L2_WORK_UPDATE_L2;
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_SEND_DRAM_REQ) {
            if (dram_req->did_win_last_arbitration) {
                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent a dram request for feed of data " 
                          << start_maddr << endl;
                dram_req->did_win_last_arbitration = false;
                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                entry->status = _L2_WORK_WAIT_DRAM_REP;
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_WAIT_DRAM_REP) {
            if (dram_rep) {
                dram_rep->did_win_last_arbitration = true;
                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }

                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] received a dram reply for " << start_maddr << endl;

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_network_plus_serialization_cost(system_time - dram_rep->milestone_time);
                }

                if (m_cfg.save_timestamp_in_dram && dram_rep->req->read_aux()) {
                    /* restore timestamp information */

                    line_info = static_pointer_cast<coherenceInfo>(dram_rep->req->read_aux());

                    if (lcc_write_req) {
                        /* WRITE */
                        if (m_cfg.logic == TIMESTAMP_IDEAL) {
                            /* expire all timestamps */
                            if (*line_info->synched_expiration_time == UINT64_MAX) {
                                *line_info->synched_expiration_time = system_time;
                                line_info->directory->clear();
                                *line_info->first_read_time_in_phase = UINT64_MAX;
                                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] has invalidated all shared lines for "
                                    << start_maddr << " due to a write." << " firt read time since last expiration : " 
                                    << hex << *line_info->first_read_time_in_phase << dec << endl;
                            }
                        } else if (line_info->expiration_time > system_time) {
                            /* we can't write now */
                            entry->status = _L2_WORK_WAIT_TIMESTAMP_AFTER_DRAM_FEED;
                            entry->accept_read_requests = true;

                            /* cost breakdown study */
                            if (stats_enabled()) {
                                stats()->write_blocked_by_timestamp();
                                stats()->add_l2_write_block_cost(line_info->expiration_time - system_time);
                            }
                            entry->milestone_time = line_info->expiration_time;

                            ++it_addr;
                            continue;
                        }

                        /* perform the write before updating the cache */
                        uint32_t word_offset = (lcc_write_req->maddr.address / 4 ) % m_cfg.words_per_cache_line;
                        for (uint32_t i = 0; i < lcc_write_req->word_count; ++i) {
                            dram_rep->req->read()[i + word_offset] = lcc_write_req->data[i];
                        }

                        *line_info->last_write_time = system_time;

                    } else {
                        /* READ */
                        if (m_cfg.logic == TIMESTAMP_IDEAL) {
                            if (*line_info->synched_expiration_time <= system_time) {
#ifdef MAX_IDEAL
                                line_info->synched_expiration_time = shared_ptr<uint64_t>(new uint64_t(system_time + MAX_IDEAL));
#else
                                line_info->synched_expiration_time = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
#endif
                                *line_info->first_read_time_in_phase = system_time;
                                *line_info->num_reads_in_phase = 0;
                            } else if (*line_info->first_read_time_in_phase == UINT64_MAX) {
                                *line_info->first_read_time_in_phase = system_time;
                            }
                        } else if (m_cfg.logic == TIMESTAMP_FIXED) {
                            if (!entry->write_requests_waiting) {
                                line_info->expiration_time = system_time + m_cfg.default_timestamp_delta;
                            }
                        } else if (m_cfg.logic == TIMESTAMP_ZERO_DELAY) {
                            /* TODO : SMART */
                            /* read after dram feed, information restored from DRAM */
                        }
                        *line_info->last_read_time = system_time;
                        *line_info->num_reads_in_phase += 1;
                        line_info->directory->insert(lcc_read_req->sender);
                    }
                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, 
                                                                       CACHE_REQ_UPDATE, 
                                                                       m_cfg.words_per_cache_line,
                                                                       dram_rep->req->read(),
                                                                       dram_rep->req->read_aux()));
                    l2_req->set_milestone_time(system_time);
                    l2_req->set_reserve(true);
                    l2_req->set_clean_write(lcc_read_req);
                    entry->l2_req = l2_req;
                    entry->status = _L2_WORK_UPDATE_L2;

                } else {
                    
                    /* creating new line information */

                    line_info = shared_ptr<coherenceInfo>(new coherenceInfo);

                    line_info->synched_expiration_time = shared_ptr<uint64_t>();
                    line_info->first_read_time_in_phase = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
                    line_info->last_read_time = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
                    line_info->last_write_time = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
                    line_info->num_reads_in_phase = shared_ptr<uint64_t>(new uint64_t(0));
                    line_info->directory = shared_ptr<set<uint32_t> >(new set<uint32_t>());

                    if (lcc_write_req) {
                        *line_info->last_write_time = system_time;
                    } else {
                        *line_info->last_read_time = system_time;
                        *line_info->first_read_time_in_phase = system_time;
                        *line_info->num_reads_in_phase = 1;
                        line_info->directory->insert(lcc_read_req->sender);
                    }

                    if (m_cfg.logic == TIMESTAMP_IDEAL) {

                        if (lcc_write_req) {
                            line_info->synched_expiration_time = shared_ptr<uint64_t>(new uint64_t(system_time));
                        } else {
#ifdef MAX_IDEAL
                            line_info->synched_expiration_time = shared_ptr<uint64_t>(new uint64_t(system_time + MAX_IDEAL));
#else
                            line_info->synched_expiration_time = shared_ptr<uint64_t>(new uint64_t(UINT64_MAX));
#endif

#ifdef PRINT_IDEAL
                            cout << "(f) " << start_maddr << " @ " << system_time << " " 
                                << line_info->directory->size()
                                << endl;
#endif
                        }

                    } else if (m_cfg.logic == TIMESTAMP_FIXED) {

                        if (lcc_write_req) {
                            line_info->expiration_time = system_time;
                        } else {
                            line_info->expiration_time = system_time + m_cfg.default_timestamp_delta;
                        }

                    } else if (m_cfg.logic == TIMESTAMP_ZERO_DELAY) {
                        /* new line */
                        *line_info->last_write_time = system_time;

                        if (lcc_write_req) {
                            line_info->expiration_time = system_time;
                        } else {
                            line_info->expiration_time = system_time + LR;
                        }
                    }
                        
                    if (lcc_write_req) {
                        uint32_t word_offset = (lcc_write_req->maddr.address / 4 ) % m_cfg.words_per_cache_line;
                        for (uint32_t i = 0; i < lcc_write_req->word_count; ++i) {
                            dram_rep->req->read()[i + word_offset] = lcc_write_req->data[i];
                        }
                    }
                    l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, 
                                                                       CACHE_REQ_UPDATE, 
                                                                       m_cfg.words_per_cache_line,
                                                                       dram_rep->req->read(),
                                                                       line_info));
                    l2_req->set_reserve(true);
                    l2_req->set_clean_write(lcc_read_req);

                    /* cost breakdown study */
                    l2_req->set_milestone_time(system_time);

                    entry->l2_req = l2_req;
                    entry->status = _L2_WORK_UPDATE_L2;
                }
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_WAIT_TIMESTAMP) {
            mh_assert(m_cfg.logic != TIMESTAMP_IDEAL);  /* never reache here if we use the ideal scheme */
            if (system_time >= line_info->expiration_time) {
                entry->accept_read_requests = false;
            }
            if (lcc_read_req) {
                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] answers read request for " << start_maddr 
                          << " from " << lcc_read_req->sender << " while blocking a write request " << endl;
                lcc_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                lcc_rep->sender = m_id;
                lcc_rep->receiver = lcc_read_req->sender;
                lcc_rep->type = READ_REP;
                lcc_rep->did_win_last_arbitration = false;
                lcc_rep->word_count = lcc_read_req->word_count;
                lcc_rep->maddr = start_maddr;
                lcc_rep->data = line->data;
                lcc_rep->timestamp = line_info->expiration_time;
                lcc_rep->synched_timestamp = line_info->synched_expiration_time;
                lcc_rep->waited = 0;
                entry->lcc_rep = lcc_rep;

                line_info->directory->insert(lcc_read_req->sender);

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - lcc_read_req->milestone_time);
                }
                lcc_rep->milestone_time = system_time;

                if (lcc_rep->receiver == m_id) {
                    mh_assert(m_l1_work_table.count(start_maddr) &&
                              m_l1_work_table[start_maddr]->status == _L1_WORK_WAIT_LCC_REP);
                    m_l1_work_table[start_maddr]->lcc_rep = lcc_rep;
                    lcc_rep->did_win_last_arbitration = true;
                    if (stats_enabled()) {
                        stats()->did_read_l2(true);
                    }
                    entry->lcc_read_req = shared_ptr<coherenceMsg>();
                } else {
                    entry->accept_read_requests = false; /* enable when it comes back */
                    msg_to_send = shared_ptr<message_t>(new message_t);
                    msg_to_send->type = MSG_LCC_REP;
                    msg_to_send->src = m_id;
                    msg_to_send->dst = lcc_rep->receiver;
                    /* always reads here */
                    msg_to_send->flit_count = get_flit_count(1 + (m_cfg.words_per_cache_line + TIMESTAMP_WORDS) * 4);
                    msg_to_send->content = lcc_rep;
                    entry->net_msg_to_send = msg_to_send;
                    m_to_network_schedule_q[MSG_LCC_REP].push_back(msg_to_send);
                    entry->status = _L2_WORK_SEND_LCC_REP_THEN_WAIT_FOR_WRITE;
                    entry->lcc_read_req = shared_ptr<coherenceMsg>();
                }
            } else if (system_time >= line_info->expiration_time) {

                /* cost breakdown study */
                if(stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - entry->milestone_time);
                }

                /* update the line before writing to the L2 */
                uint32_t word_offset = (lcc_write_req->maddr.address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < lcc_write_req->word_count; ++i) {
                    line->data[i + word_offset] = lcc_write_req->data[i];
                }
                shared_ptr<cacheRequest> new_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line, 
                                                                   line->data, line_info));
                new_req->set_clean_write(false);
                new_req->set_reserve(true);
                new_req->set_requesting_core(lcc_write_req->sender);
                new_req->set_milestone_time(system_time);

                entry->l2_req = new_req;

                *line_info->last_write_time = system_time;
                line_info->directory->clear();

                entry->status = _L2_WORK_UPDATE_L2;
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_WAIT_TIMESTAMP_AFTER_DRAM_FEED) {
            line_info = static_pointer_cast<coherenceInfo>(dram_rep->req->read_aux());
            mh_assert(m_cfg.logic != TIMESTAMP_IDEAL);  /* never reache here if we use the ideal scheme */
            if (system_time >= line_info->expiration_time) {
                entry->accept_read_requests = false;
            }
            if (lcc_read_req) {
                mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] answers read request for " << start_maddr 
                          << " from " << lcc_read_req->sender << " while blocking a write request " << endl;
                lcc_rep = shared_ptr<coherenceMsg>(new coherenceMsg);
                lcc_rep->sender = m_id;
                lcc_rep->receiver = lcc_read_req->sender;
                lcc_rep->type = READ_REP;
                lcc_rep->did_win_last_arbitration = false;
                lcc_rep->word_count = m_cfg.words_per_cache_line;
                lcc_rep->maddr = start_maddr;
                lcc_rep->data = dram_rep->req->read();
                lcc_rep->timestamp = line_info->expiration_time;
                lcc_rep->synched_timestamp = line_info->synched_expiration_time;
                lcc_rep->waited = 0;
                entry->lcc_rep = lcc_rep;

                line_info->directory->insert(lcc_read_req->sender);

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - lcc_read_req->milestone_time);
                }
                lcc_rep->milestone_time = system_time;

                if (lcc_rep->receiver == m_id) {
                    mh_assert(m_l1_work_table.count(start_maddr) &&
                              m_l1_work_table[start_maddr]->status == _L1_WORK_WAIT_LCC_REP);
                    m_l1_work_table[start_maddr]->lcc_rep = lcc_rep;
                    lcc_rep->did_win_last_arbitration = true;
                    if (stats_enabled()) {
                        stats()->did_read_l2(true);
                    }
                    entry->lcc_read_req = shared_ptr<coherenceMsg>();
                } else {
                    entry->accept_read_requests = false; /* enable when it comes back */
                    msg_to_send = shared_ptr<message_t>(new message_t);
                    msg_to_send->type = MSG_LCC_REP;
                    msg_to_send->src = m_id;
                    msg_to_send->dst = lcc_rep->receiver;
                    /* always reads here */
                    msg_to_send->flit_count = get_flit_count(1 + (m_cfg.words_per_cache_line + TIMESTAMP_WORDS) * 4);
                    msg_to_send->content = lcc_rep;
                    entry->net_msg_to_send = msg_to_send;
                    m_to_network_schedule_q[MSG_LCC_REP].push_back(msg_to_send);
                    entry->status = _L2_WORK_SEND_LCC_REP_THEN_WAIT_FOR_WRITE_AFTER_DRAM_FEED;
                    entry->lcc_read_req = shared_ptr<coherenceMsg>();
                }
            } else if (system_time >= line_info->expiration_time) {
                /* update the line before writing to the L2 */
                uint32_t word_offset = (lcc_write_req->maddr.address / 4 ) % m_cfg.words_per_cache_line;
                for (uint32_t i = 0; i < lcc_write_req->word_count; ++i) {
                    dram_rep->req->read()[i + word_offset] = lcc_write_req->data[i];
                }
                l2_req = shared_ptr<cacheRequest>(new cacheRequest(start_maddr, CACHE_REQ_UPDATE,
                                                                   m_cfg.words_per_cache_line, 
                                                                   dram_rep->req->read(), line_info));
                l2_req->set_clean_write(false);
                l2_req->set_milestone_time(system_time);

                entry->l2_req = l2_req;
                entry->status = _L2_WORK_UPDATE_L2;

                *line_info->last_write_time = system_time;
                line_info->directory->clear();

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - entry->milestone_time);
                }
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_SEND_LCC_REP_THEN_WAIT_FOR_WRITE) {
            if (lcc_rep->did_win_last_arbitration) {
                lcc_rep->did_win_last_arbitration = false;

                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                if (stats_enabled()) {
                    stats()->did_read_l2(true);
                }
                entry->lcc_read_req = shared_ptr<coherenceMsg>();
                entry->status = _L2_WORK_WAIT_TIMESTAMP;
                entry->accept_read_requests = true;
            } else {
                m_to_network_schedule_q[MSG_LCC_REP].push_back(msg_to_send);
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_SEND_LCC_REP_THEN_WAIT_FOR_WRITE_AFTER_DRAM_FEED) {
            if (lcc_rep->did_win_last_arbitration) {
                lcc_rep->did_win_last_arbitration = false;
                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                if (stats_enabled()) {
                    stats()->did_read_l2(true);
                }
                entry->lcc_read_req = shared_ptr<coherenceMsg>();
                entry->status = _L2_WORK_WAIT_TIMESTAMP_AFTER_DRAM_FEED;
                entry->accept_read_requests = true;
            } else {
                m_to_network_schedule_q[MSG_LCC_REP].push_back(msg_to_send);
            }
            ++it_addr;
            continue;

        } else if (entry->status == _L2_WORK_SEND_LCC_REP_THEN_FINISH) {
            if (lcc_rep->did_win_last_arbitration) {
                lcc_rep->did_win_last_arbitration = false;
                if (msg_to_send) {
                    entry->net_msg_to_send = shared_ptr<message_t>();
                }
                if (entry->using_space_for_reads) {
                    ++m_l2_work_table_vacancy_readonly;
                } else {
                    ++m_l2_work_table_vacancy_shared;
                }
                if (lcc_rep->type == READ_REP) {
                    mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent an LCC read reply for " 
                        << start_maddr << " to " << lcc_rep->receiver << " timestamp ";
                    if (lcc_rep->synched_timestamp) {
                        mh_log(4) << *lcc_rep->synched_timestamp << endl;
                    } else {
                        mh_log(4) << lcc_rep->timestamp << endl;
                    }

                    if (stats_enabled()) {
                        stats()->did_read_l2(!entry->did_miss_on_first);
                    }
                } else {
                    mh_log(4) << "[LCC " << m_id << " @ " << system_time << " ] sent an LCC write reply for " 
                        << lcc_write_req->maddr << " to " << lcc_rep->receiver << endl;
                    if (stats_enabled()) {
                        stats()->did_write_l2(!entry->did_miss_on_first);
                    }
                }
                m_l2_work_table.erase(it_addr++);
                continue;
            } else {
                m_to_network_schedule_q[MSG_LCC_REP].push_back(msg_to_send);
                ++it_addr;
                continue;
            }
        }
        ++it_addr;
    }
}

void privateSharedLCC::dram_work_table_update() {
    for (toDRAMTable::iterator it_addr = m_dram_work_table.begin(); it_addr != m_dram_work_table.end(); ) {
        shared_ptr<toDRAMEntry> entry = it_addr->second;
        if (entry->dram_req->req->status() == DRAM_REQ_DONE) {

            if (!entry->dram_rep) {
                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_dram_offchip_network_plus_dram_action_cost(system_time - entry->milestone_time);
                }

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
                mh_assert(m_l2_work_table.count(start_maddr) && m_l2_work_table[start_maddr]->status == _L2_WORK_WAIT_DRAM_REP);
                m_l2_work_table[start_maddr]->dram_rep = entry->dram_rep;
                mh_log(4) << "[DRAM " << m_id << " @ " << system_time << " ] has sent a dram rep for address " 
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

void privateSharedLCC::accept_incoming_messages() {

    while (m_core_receive_queues[MSG_LCC_REP]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_LCC_REP]->front();
        shared_ptr<coherenceMsg> lcc_msg = static_pointer_cast<coherenceMsg>(msg->content);
        maddr_t msg_start_maddr = get_start_maddr_in_line(lcc_msg->maddr);
        if (m_l1_work_table.count(msg_start_maddr) == 0) {
            cerr << "[L1 " << m_id << " @ " << system_time << " ] assertion failed. no entry for address " 
                 << lcc_msg->maddr << " (start maddr: " << msg_start_maddr << " ) upon a reply " << endl;
        }
        if (m_l1_work_table[msg_start_maddr]->status != _L1_WORK_WAIT_LCC_REP) {
            cerr << "[L1 " << m_id << " @ " << system_time << " ] assertion failed. entry for address " 
                 << lcc_msg->maddr << " (start maddr: " << msg_start_maddr << " ) is in state " 
                 << m_l1_work_table[msg_start_maddr]->status << " upon a reply " << endl;
        }
        mh_assert(m_l1_work_table.count(msg_start_maddr) && 
                  (m_l1_work_table[msg_start_maddr]->status == _L1_WORK_WAIT_LCC_REP));
        m_l1_work_table[msg_start_maddr]->lcc_rep = lcc_msg;
        m_core_receive_queues[MSG_LCC_REP]->pop();
        break;
    }
    
    while (m_core_receive_queues[MSG_LCC_REQ_1]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_LCC_REQ_1]->front();
        shared_ptr<coherenceMsg> lcc_msg = static_pointer_cast<coherenceMsg>(msg->content);

        if (lcc_msg->did_win_last_arbitration) {
            /* reset the flag for the next arbitration */
            lcc_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_LCC_REQ_1]->pop();
            /* this pop is supposed to be done in the previous cycle. continue to the next cycle */
            continue;
        } else {
            /* erase later */

#ifdef DEADLOCK_CHECK
            if (++lcc_msg->waited > 30000) {
                maddr_t msg_start_maddr = get_start_maddr_in_line(lcc_msg->maddr);
                cerr << "[NET " << m_id << " @ " << system_time << " ] cannot receive a cache req for start maddr " 
                     << msg_start_maddr << " (table:size " << m_l2_work_table.size() << " ) ";
                if (m_l2_work_table.count(msg_start_maddr)) {
                    cerr << "existing entry state : " << m_l2_work_table[msg_start_maddr]->status;
                    if (m_l2_work_table[msg_start_maddr]->status == _L2_WORK_WAIT_TIMESTAMP) {
                        cerr << " cur timestamp " << get_expiration_time(m_l2_work_table[msg_start_maddr]->l2_req->line_copy());
                    }
                    cerr << endl;
                } else {
                    cerr << "table full" << endl;
                }
            }
            if (lcc_msg->waited > 31000) {
                throw err_bad_shmem_cfg("seems like a deadlock");
            }
#endif
            m_to_l2_req_schedule_q.push_back(lcc_msg);
            break;
        }
    }

    while (m_cfg.use_separate_vc_for_writes && m_core_receive_queues[MSG_LCC_REQ_2]->size()) {
        shared_ptr<message_t> msg = m_core_receive_queues[MSG_LCC_REQ_2]->front();
        shared_ptr<coherenceMsg> lcc_msg = static_pointer_cast<coherenceMsg>(msg->content);
        if (lcc_msg->did_win_last_arbitration) {
            /* reset the flag for the next arbitration */
            lcc_msg->did_win_last_arbitration = false;
            m_core_receive_queues[MSG_LCC_REQ_2]->pop();
            /* this pop is supposed to be done in the previous cycle. continue to the next cycle */
            continue;
        } else {
            /* erase later */
#ifdef DEADLOCK_CHECK
            if (++lcc_msg->waited > 30000) {
                maddr_t msg_start_maddr = get_start_maddr_in_line(lcc_msg->maddr);
                cerr << "[NET " << m_id << " @ " << system_time << " ] cannot receive a cache write req for start maddr "
                     << msg_start_maddr << " from " << msg->src << " cannot get in"  
                     << " (table:size " << m_l2_work_table.size() << " ) ";
                if (m_l2_work_table.count(msg_start_maddr)) {
                    cerr << "existing entry state : " << m_l2_work_table[msg_start_maddr]->status;
                    if (m_l2_work_table[msg_start_maddr]->status == _L2_WORK_WAIT_TIMESTAMP) {
                        cerr << " cur timestamp " << get_expiration_time(m_l2_work_table[msg_start_maddr]->l2_req->line_copy());
                    }
                    cerr << endl;
                } else {
                    cerr << "table full" << endl;
                }
            }
            if (lcc_msg->waited > 31000) {
                throw err_bad_shmem_cfg("seems like a deadlock");
            }
#endif
            m_to_l2_req_schedule_q.push_back(lcc_msg);
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
        mh_assert(m_l2_work_table.count(start_maddr) > 0 && m_l2_work_table[start_maddr]->status == _L2_WORK_WAIT_DRAM_REP);
        m_l2_work_table[start_maddr]->dram_rep = dram_msg;
        m_core_receive_queues[MSG_DRAM_REP]->pop();
    }

}

void privateSharedLCC::schedule_requests() {


    /* random arbitration */
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);

    /* 1 : arbitrates requests from the core. */
    random_shuffle(m_core_port_schedule_q.begin(), m_core_port_schedule_q.end(), rr_fn);
    while (m_core_port_schedule_q.size()) {
        shared_ptr<memoryRequest> req = m_core_port_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(req->maddr());
        if (m_available_core_ports == 0) {
            set_req_status(req, REQ_RETRY);
            m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
            continue;
        }
        if (m_l1_work_table.count(start_maddr) > 0) {
            set_req_status(req, REQ_RETRY);
            m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());
            continue;
        }

        shared_ptr<toL1Entry> new_entry(new toL1Entry);
        new_entry->core_req = req;

        if (req->is_read()) {
            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a read request from core for "
                      << req->maddr() << " (start maddr : " << start_maddr << " ) " << endl;
            shared_ptr<cacheRequest> l1_req(new cacheRequest(req->maddr(), 
                                                             CACHE_REQ_READ,
                                                             req->word_count(),
                                                             shared_array<uint32_t>()));
            l1_req->set_reserve(false);
            l1_req->set_milestone_time(system_time);
            new_entry->l1_req = l1_req;
            new_entry->status = _L1_WORK_WAIT_L1;
        } else {
            mh_log(4) << "[L1 " << m_id << " @ " << system_time << " ] received a write request from core for "
                      << req->maddr() << " (start maddr : " << start_maddr << " ) " << endl;
#ifdef PRIVATE_BYPASS
            shared_ptr<cacheRequest> l1_req(new cacheRequest(req->maddr(), 
                                                             CACHE_REQ_WRITE,
                                                             req->word_count(),
                                                             req->data()));
            l1_req->set_reserve(false);
            l1_req->set_milestone_time(system_time);
            new_entry->l1_req = l1_req;
            new_entry->status = _L1_WORK_WAIT_L1;
#else
            new_entry->status = _L1_WORK_WAIT_CAT;
#endif
        }

        shared_ptr<catRequest> cat_req(new catRequest(req->maddr(), m_id));
        new_entry->cat_req = cat_req;

        new_entry->lcc_rep = shared_ptr<coherenceMsg>();
        new_entry->requested_time = system_time;
        new_entry->net_msg_to_send = shared_ptr<message_t>();

        /* cost breakdown study */
        if (stats_enabled()) {
            stats()->add_memory_subsystem_serialization_cost(system_time - req->milestone_time());
        }
        new_entry->milestone_time = system_time;
        if (req->is_read()) {
            cat_req->set_milestone_time(UINT64_MAX); /* count CAT cost only if it's not hiden by L1 latency */
        } else {
            cat_req->set_milestone_time(system_time);
        }

        set_req_status(req, REQ_WAIT);
        m_l1_work_table[start_maddr] = new_entry;
        --m_available_core_ports;

        m_core_port_schedule_q.erase(m_core_port_schedule_q.begin());

    }

    random_shuffle(m_to_l2_req_schedule_q.begin(), m_to_l2_req_schedule_q.end(), rr_fn);
    while (m_to_l2_req_schedule_q.size()) {
        shared_ptr<coherenceMsg> msg = m_to_l2_req_schedule_q.front();
        maddr_t start_maddr = get_start_maddr_in_line(msg->maddr);
        if (msg->type == WRITE_REQ) {
            if (m_l2_work_table_vacancy_shared > 0 && m_l2_work_table.count(start_maddr) == 0) {
                mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received an LCC write request from " << msg->sender 
                          << " for address " << msg->maddr << " (new entry - reserved) " << endl;

                shared_ptr<toL2Entry> new_entry(new toL2Entry);
                new_entry->accept_read_requests = false;
                new_entry->write_requests_waiting = false;
                new_entry->waiting_for_evictions = false;

                --m_l2_work_table_vacancy_shared;
                new_entry->using_space_for_reads = false;

                new_entry->status = _L2_WORK_WAIT_L2;
                new_entry->did_miss_on_first = false;

                new_entry->lcc_write_req = msg;
                new_entry->lcc_read_req = shared_ptr<coherenceMsg>();

                shared_ptr<cacheRequest> l2_req(new cacheRequest(msg->maddr, CACHE_REQ_WRITE, 
                                                                 msg->word_count,
                                                                 msg->data));
                l2_req->set_reserve(true);
                l2_req->set_clean_write(false);
                new_entry->l2_req = l2_req;

                l2_req->set_requesting_core(msg->sender);

                new_entry->lcc_rep = shared_ptr<coherenceMsg>();
                new_entry->dram_req = shared_ptr<dramMsg>();
                new_entry->dram_rep = shared_ptr<dramMsg>();
                new_entry->net_msg_to_send = shared_ptr<message_t>();

                msg->did_win_last_arbitration = true;

                m_l2_work_table[start_maddr] = new_entry;

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - msg->milestone_time);
                }
                l2_req->set_milestone_time(system_time);
                new_entry->milestone_time = system_time;

            } else if (m_l2_work_table.count(start_maddr)) {
                m_l2_work_table[start_maddr]->write_requests_waiting = true;
            }
            m_to_l2_req_schedule_q.erase(m_to_l2_req_schedule_q.begin());
        } else {
            if (m_l2_work_table.count(start_maddr)) {
                if (m_l2_work_table[start_maddr]->accept_read_requests && !m_l2_work_table[start_maddr]->lcc_read_req) {
                    mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received an LCC read request" 
                              << " from " << msg->sender << " for address " << msg->maddr << endl;
                    m_l2_work_table[start_maddr]->lcc_read_req = msg;
                    msg->did_win_last_arbitration = true;
                }
            } else if (m_l2_work_table_vacancy_readonly > 0 || m_l2_work_table_vacancy_shared > 0) {
                mh_log(4) << "[MEM " << m_id << " @ " << system_time << " ] received an LCC read request from " << msg->sender 
                          << " for address " << msg->maddr << " (new entry - reserved) " << endl;
                shared_ptr<toL2Entry> new_entry(new toL2Entry);
                new_entry->accept_read_requests = false;
                new_entry->write_requests_waiting = false;
                new_entry->waiting_for_evictions = false;
                if (m_l2_work_table_vacancy_readonly > 0) {
                    --m_l2_work_table_vacancy_readonly;
                    new_entry->using_space_for_reads = true;
                } else {
                    --m_l2_work_table_vacancy_shared;
                    new_entry->using_space_for_reads = false;
                }
                new_entry->status = _L2_WORK_WAIT_L2;
                new_entry->did_miss_on_first = false;

                new_entry->lcc_read_req = msg;
                new_entry->lcc_write_req = shared_ptr<coherenceMsg>();

                shared_ptr<cacheRequest> l2_req(new cacheRequest(start_maddr, CACHE_REQ_READ, m_cfg.words_per_cache_line));
                l2_req->set_reserve(true);
                new_entry->l2_req = l2_req;

                l2_req->set_requesting_core(msg->sender);

                new_entry->lcc_rep = shared_ptr<coherenceMsg>();
                new_entry->dram_req = shared_ptr<dramMsg>();
                new_entry->dram_rep = shared_ptr<dramMsg>();
                new_entry->net_msg_to_send = shared_ptr<message_t>();

                /* cost breakdown study */
                if (stats_enabled()) {
                    stats()->add_l2_network_plus_serialization_cost(system_time - msg->milestone_time);
                }
                new_entry->milestone_time = system_time;
                l2_req->set_milestone_time(system_time);

                msg->did_win_last_arbitration = true;

                m_l2_work_table[start_maddr] = new_entry;

            }
            m_to_l2_req_schedule_q.erase(m_to_l2_req_schedule_q.begin());

        }
    }

    /* 4 : arbitrate inputs to dram work table */
    random_shuffle(m_to_dram_req_schedule_q.begin(), m_to_dram_req_schedule_q.end(), rr_fn);
    while (m_to_dram_req_schedule_q.size()) {
        mh_assert(m_dram_controller);
        shared_ptr<dramMsg> msg = m_to_dram_req_schedule_q.front();
        if (m_dram_controller->available()) {
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
            m_dram_controller->request(msg->req);
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
        shared_ptr<catRequest> cat_req = m_cat_req_schedule_q.front();
        m_cat->request(cat_req);

        /* cost breakdown study */
        if (cat_req->milestone_time() != UINT64_MAX) {
            if (stats_enabled()) {
                stats()->add_cat_serialization_cost(system_time - cat_req->milestone_time());
            }
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
            if (stats_enabled()) {
                stats()->add_l1_serialization_cost(system_time -  l1_req->milestone_time());
            }
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
            if (stats_enabled()) {
                stats()->add_l1_serialization_cost(system_time - l1_req->milestone_time());
            }
            l1_req->set_milestone_time(system_time);
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
                stats()->add_l2_network_plus_serialization_cost(system_time - req->milestone_time());
            }
            req->set_milestone_time(system_time);

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
            stats()->add_l2_network_plus_serialization_cost(system_time - req->milestone_time());
        }
        req->set_milestone_time(system_time);

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
                case MSG_LCC_REQ_1:
                case MSG_LCC_REQ_2:
                case MSG_LCC_REP:
                    {
                        shared_ptr<coherenceMsg> lcc_msg = static_pointer_cast<coherenceMsg>(msg->content);
                        lcc_msg->did_win_last_arbitration = true;
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
