// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cache.hpp"
#include "stdio.h"
#include <boost/function.hpp>
#include <boost/bind.hpp>

#define DEBUG

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(m_id==27) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X)
#define mh_log(X) LOG(log,X)
#endif

cacheRequest::cacheRequest(maddr_t maddr, cacheReqType_t request_type, uint32_t word_count,
                           shared_array<uint32_t> data_to_write, shared_ptr<void> coherence_info_to_write) :
    m_request_type(request_type), 
    m_maddr(maddr), 
    m_word_count(word_count),
    m_status(CACHE_REQ_NEW), 
    m_line_copy(shared_ptr<cacheLine>()), 
    m_line_to_evict_copy(shared_ptr<cacheLine>()),
    m_coherence_info_to_write(coherence_info_to_write), 
    m_data_to_write(data_to_write),
    m_aux_info_for_coherence(shared_ptr<void>()),
    m_do_unset_dirty_on_write(false),
    m_do_claim(true), 
    m_do_evict(true)
{}

cacheRequest::~cacheRequest() {}

cache::cache(uint32_t level, uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r, 
             uint32_t words_per_line, uint32_t total_lines, uint32_t associativity, replacementPolicy_t replacement_policy,
             uint32_t hit_test_latency, uint32_t num_read_ports,  uint32_t num_write_ports) : 
    m_level(level), 
    m_id(id), 
    system_time(t), 
    stats(st), 
    log(l), 
    ran(r), 
    m_words_per_line(words_per_line), 
    m_total_lines(total_lines),
    m_associativity(associativity), 
    m_replacement_policy(replacement_policy), 
    m_hit_test_latency(hit_test_latency),
    m_available_read_ports(num_read_ports), 
    m_available_write_ports(num_write_ports),
    m_helper_copy_coherence_info(NULL), 
    m_helper_is_coherence_hit(NULL), 
    m_helper_can_evict_line(NULL),
    m_helper_evict_need_action(NULL),
    m_helper_replacement_policy(NULL), 
    m_claim_hook(NULL),
    m_invalidate_hook(NULL),
    m_read_hook(NULL),
    m_write_hook(NULL),
    m_update_hook(NULL)
{ 
    mh_assert(m_associativity > 0);
    mh_assert(m_words_per_line > 0);
    mh_assert(m_available_read_ports > 0);
    mh_assert(m_available_write_ports > 0);
    int log2 = 0;
    for (uint32_t i = m_words_per_line * 4; i > 1; i = i/2) {
        mh_assert(i%2 == 0); /*  must be a power of 2 */
        ++log2;
    }
    m_offset_mask = m_words_per_line * 4 - 1;
    m_index_pos = log2;
    m_index_mask = (m_total_lines / m_associativity - 1) << m_index_pos;

}

cache::~cache() {
    for (cacheTable::iterator it = m_cache.begin(); it != m_cache.end(); ++it) {
        delete[] m_cache[it->first];
    }
}

void cache::print_contents() {
    cout << "[cache " << m_id << " - L" << m_level << " ] printing cache contents" << endl;
    for (cacheTable::iterator it = m_cache.begin(); it != m_cache.end(); ++it) {
        uint32_t index = it->first;
        for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
            cout << "  (idx=" << index << ", way=" << it_way << ") : "; 
            if (m_cache[index][it_way].claimed) {
                cout << "[" << m_cache[index][it_way].start_maddr << "] " << endl;
                cout << hex;
                for (uint32_t o = 0; o < m_words_per_line; o++) {
                    cout << m_cache[index][it_way].data[o] << " ";
                }
                cout << dec << endl;
            } else {
                cout << "empty" << endl;
            }
        }
    }
    cout << endl;
}

shared_ptr<cacheLine> cache::copy_cache_line(const cacheLine &line) {

    /* the cache always returns copies of data, not pointers */
    /* For coherence information, shmem systems decide what to do */
    /* For example, it may do shallow copy to implement some magic hardware, or concurrent snoopy mechanisms */

    shared_ptr<cacheLine> copy(new cacheLine);
    (*copy) = line;

    /* deep copy data */
    if (line.data) {
        copy->data = shared_array<uint32_t>(new uint32_t[m_words_per_line]);
        for (uint32_t i = 0; i < m_words_per_line; ++i) {
            copy->data[i] = line.data[i];
        }
    }

    /* let the helper function deal with coherence information */
    if (line.coherence_info) {
        mh_assert(m_helper_copy_coherence_info);
        copy->coherence_info = m_helper_copy_coherence_info(line.coherence_info);
    }

    return copy;
}

void cache::request(shared_ptr<cacheRequest> req) {

    req->m_status = CACHE_REQ_WAIT;

    shared_ptr<reqEntry> new_entry(new reqEntry);
    new_entry->status = (m_hit_test_latency > 0) ? ENTRY_HIT_TEST : ENTRY_DONE;
    new_entry->request = req;
    new_entry->start_maddr = get_start_maddr_in_line(req->m_maddr);
    new_entry->idx = get_index(req->m_maddr);
    new_entry->remaining_hit_test_cycles = m_hit_test_latency;

    m_req_table[new_entry->start_maddr].push_back(new_entry);

    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
              << " a request " << req->m_request_type << " on " << req->m_maddr << " arrived "  << endl;

    if (m_req_table[new_entry->start_maddr].front() == new_entry && new_entry->status == ENTRY_DONE) {
        mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                  << " a request " << req->m_request_type << " on " << req->m_maddr << " is in the ready queue "
                  << "(queue size " << m_ready_requests.size() << " )." << endl;
        m_ready_requests.push_back(new_entry);
    }

    if (req->use_read_ports()) {
        mh_assert(read_port_available());
        --m_available_read_ports;
    } else {
        mh_assert(write_port_available());
        --m_available_write_ports;
    }

    return;

}
    
void cache::tick_positive_edge() {

    static boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);
    random_shuffle(m_ready_requests.begin(), m_ready_requests.end(), rr_fn);

    for (vector<shared_ptr<reqEntry> >::iterator it_req = m_ready_requests.begin(); it_req != m_ready_requests.end(); ) {

        shared_ptr<reqEntry> entry = *it_req;
        shared_ptr<cacheRequest> req = entry->request;
        uint32_t idx = entry->idx;
        maddr_t& start_maddr = entry->start_maddr;

        mh_log(5) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                  << " is working on a request " << req->m_request_type 
                  << " on " << req->m_maddr << endl;

        /* allocate memory for this index if not exist */
        if (m_cache.count(idx) == 0) {
            m_cache[idx] = new cacheLine[m_associativity];
            for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                m_cache[idx][it_way].claimed = false;
                m_cache[idx][it_way].data = shared_array<uint32_t>(new uint32_t[m_words_per_line]);
                m_cache[idx][it_way].coherence_info = shared_ptr<void>();
            }
        }

        /* find a matching way and/or empty ways */
        bool matched = false;
        uint32_t matching_way = 0;
        bool has_empty_way = false;
        uint32_t empty_way = 0;
        for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
            cacheLine &line = m_cache[idx][it_way];
            if (line.claimed && start_maddr == line.start_maddr) {
                /* found */
                matched = true;
                matching_way = it_way;
                break;
            } else if (!line.claimed) {
                has_empty_way = true;
                empty_way = it_way;
            }
        }

        if (matched) {
            cacheLine &line = m_cache[idx][matching_way];
            line.last_access_time = system_time;

            if (req->m_request_type == CACHE_REQ_READ || req->m_request_type == CACHE_REQ_WRITE) {
                bool coherence_hit = line.ready && (!m_helper_is_coherence_hit 
                                                    || !line.coherence_info 
                                                    || (*m_helper_is_coherence_hit)(req, line, system_time));
                if (coherence_hit) {
                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request " << req->m_request_type 
                              << " on " << req->m_maddr << " gets a HIT." << endl;
                    req->m_status = CACHE_REQ_HIT; 
                    if (req->m_request_type == CACHE_REQ_WRITE) {
                        if (req->m_word_count) {
                            uint32_t offset = get_offset(req->m_maddr) / 4;
                            for (uint32_t i = 0; i < req->m_word_count; ++i) {
                                line.data[offset + i] = req->m_data_to_write[i];
                            }
                            if (req->m_do_unset_dirty_on_write) {
                                line.data_dirty = false;
                            } else {
                                line.data_dirty = true; 
                            }
                        }
                        if (req->m_coherence_info_to_write) {
                            line.coherence_info = req->m_coherence_info_to_write;
                            if (req->m_do_unset_dirty_on_write) {
                                line.coherence_info_dirty = false;
                            } else {
                                line.coherence_info_dirty = true; 
                            }
                        }
                        if (m_write_hook) { (*m_write_hook)(line, system_time); }
                    } else {
                        if (m_read_hook) { (*m_read_hook)(line, system_time); }
                    }
                } else {
                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request " << req->m_request_type 
                              << " on " << req->m_maddr << " gets a coherence or not-ready MISS." << endl;
                    req->m_status = CACHE_REQ_MISS; 
                }

                req->m_line_copy = copy_cache_line(line);

            } else if (req->m_request_type == CACHE_REQ_UPDATE) {

                bool coherence_hit = line.ready && (!m_helper_is_coherence_hit 
                                                    || !line.coherence_info 
                                                    || (*m_helper_is_coherence_hit)(req, line, system_time));


                if (!line.ready || coherence_hit) {

                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request UPDATE "  
                              << " on " << req->m_maddr << " gets a HIT." << endl;

                    line.ready = true;
                    line.data_dirty = false;
                    line.coherence_info_dirty = false;

                    req->m_status = CACHE_REQ_HIT; 
                    if (req->m_word_count > 0) {
                        uint32_t offset = get_offset(req->m_maddr) / 4;
                        for (uint32_t i = 0; i < req->m_word_count; ++i) { 
                            line.data[offset + i] = req->m_data_to_write[i]; 
                        }
                        if (req->m_do_unset_dirty_on_write) {
                            line.data_dirty = false;
                        } else {
                            line.data_dirty = true; 
                        }
                    }
                    if (req->m_coherence_info_to_write) {
                        line.coherence_info = req->m_coherence_info_to_write;
                        if (req->m_do_unset_dirty_on_write) {
                            line.coherence_info_dirty = false;
                        } else {
                            line.coherence_info_dirty = true; 
                        }
                    }
                    if (m_update_hook) { (*m_update_hook)(line, system_time); }
                } else {

                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request UPDATE "  
                              << " on " << req->m_maddr << " gets a coherence MISS." << endl;

                    req->m_status = CACHE_REQ_MISS;
                }

                req->m_line_copy = copy_cache_line(line);

            } else if (req->m_request_type == CACHE_REQ_INVALIDATE) {

                bool coherence_hit = line.ready && (!m_helper_is_coherence_hit 
                                                || !line.coherence_info 
                                                || (*m_helper_is_coherence_hit)(req, line, system_time));
                if (!line.ready || coherence_hit) {
                    req->m_status = CACHE_REQ_HIT;

                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request INVALIDATE "  
                              << " on " << req->m_maddr << " gets a HIT." << endl;

                    /* must act at the negative edge */
                    m_lines_to_invalidate.push_back(make_tuple(idx, matching_way, entry));

                } else {

                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request INVALIDATE "  
                              << " on " << req->m_maddr << " gets a coherence MISS." << endl;

                    req->m_status = CACHE_REQ_MISS;
                    req->m_line_copy = copy_cache_line(line);
                }
            }

            if (req->use_read_ports()) {
                ++m_available_read_ports;
            } else {
                ++m_available_write_ports;
            }

            m_ready_requests.erase(it_req);
            m_req_table[start_maddr].erase(m_req_table[start_maddr].begin());
            if (m_req_table[start_maddr].size() == 0) {
                m_req_table.erase(start_maddr);
            }
            continue;

        } else {
            /* no matching cache block */

            if (req->m_do_evict && !has_empty_way) {
                mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                          << " a request " << req->m_request_type
                          << " on " << req->m_maddr << " need to evict a line." << endl;
                ++it_req;
                continue;
            } 
            
            if (req->m_do_claim && has_empty_way) {
                cacheLine &line = m_cache[idx][empty_way];
                line.claimed = true;
                line.start_maddr = start_maddr;

                if (m_claim_hook) {
                    (*m_claim_hook)(line, system_time);
                }

                if (req->m_request_type == CACHE_REQ_UPDATE) {
                    line.ready = true;
                    line.data_dirty = false;
                    line.coherence_info_dirty = false;
                    line.last_access_time = system_time;
                    if (req->m_word_count > 0) {
                        uint32_t offset = get_offset(req->m_maddr) / 4;
                        for (uint32_t i = 0; i < req->m_word_count; ++i) { 
                            line.data[offset + i] = req->m_data_to_write[i]; 
                        }
                        if (req->m_do_unset_dirty_on_write) {
                            line.data_dirty = false;
                        } else {
                            line.data_dirty = true; 
                        }
                    }
                    if (req->m_coherence_info_to_write) {
                        line.coherence_info = req->m_coherence_info_to_write;
                        if (req->m_do_unset_dirty_on_write) {
                            line.coherence_info_dirty = false;
                        } else {
                            line.coherence_info_dirty = true; 
                        }
                    }

                    if (m_update_hook) { 
                        (*m_update_hook)(line, system_time); 
                    }

                    req->m_status = CACHE_REQ_HIT;

                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request UPDATE " 
                              << " on " << req->m_maddr << " gets a HIT on an empty block." << endl;

                } else {

                    mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                              << " a request " << req->m_request_type 
                              << " on " << req->m_maddr << " gets a MISS but claimed a line." << endl;

                    line.ready = false;
                    req->m_status = CACHE_REQ_MISS;
                }
                req->m_line_copy = copy_cache_line(line);

            } else {

                mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                          << " a request " << req->m_request_type 
                          << " on " << req->m_maddr << " gets a MISS." << endl;

                req->m_status = CACHE_REQ_MISS;
            }

            if (req->use_read_ports()) {
                ++m_available_read_ports;
            } else {
                ++m_available_write_ports;
            }
            m_ready_requests.erase(it_req);
            m_req_table[start_maddr].erase(m_req_table[start_maddr].begin());
            if (m_req_table[start_maddr].size() == 0) {
                m_req_table.erase(start_maddr);
            }
            continue;
        }
        /* never reach here */
    }

}

void cache::tick_negative_edge() {

    while (m_ready_requests.size()) {
        shared_ptr<reqEntry> entry = m_ready_requests.front();
        shared_ptr<cacheRequest> req = entry->request;
        uint32_t idx = entry->idx;
        maddr_t& start_maddr = entry->start_maddr;

        mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                  << " a request " << req->m_request_type 
                  << " on " << req->m_maddr << " tries to evict a line." << endl;

        /* no matter what, they will finish here */
        if (req->use_read_ports()) {
            ++m_available_read_ports;
        } else {
            ++m_available_write_ports;
        }
        m_req_table[start_maddr].erase(m_req_table[start_maddr].begin());
        if (m_req_table[start_maddr].size() == 0) {
            m_req_table.erase(start_maddr);
        }
        m_ready_requests.erase(m_ready_requests.begin());

        /* find a matching way and/or empty ways */
        vector<uint32_t> evictables;
        uint64_t lru_time = UINT64_MAX;
        uint32_t lru_way = 0;
        for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
            cacheLine &line = m_cache[idx][it_way];
            if (line.ready && line.last_access_time < system_time 
                && (!m_helper_can_evict_line || (*m_helper_can_evict_line)(line, system_time)) ) 
            {
                evictables.push_back(it_way);
                if (line.last_access_time < lru_time) {
                    lru_way = it_way;
                    lru_time = line.last_access_time;
                }
            }
        }

        if (evictables.empty()) {
            mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                      << " a request " << req->m_request_type 
                      << " on " << req->m_maddr << " found no evictable lines." << endl;
            req->m_status = CACHE_REQ_MISS;
            continue;
        }

        uint32_t victim_way = 0;
        switch (m_replacement_policy) {
        case REPLACE_RANDOM:
            victim_way = evictables[ran->random_range(evictables.size())];
            break;
        case REPLACE_LRU:
            victim_way = lru_way;
            break;
        case REPLACE_CUSTOM:
            mh_assert(m_helper_replacement_policy);
            victim_way = (*m_helper_replacement_policy)(evictables, m_cache[idx], system_time, ran);
            break;
        default:
            break;
        }

        cacheLine &line = m_cache[idx][victim_way];

        if (!m_helper_evict_need_action || !line.coherence_info || !(*m_helper_evict_need_action)(line, system_time)) {
            line.last_access_time = system_time; /* prevent this line to be considered again */
            m_lines_to_evict.push_back(make_tuple(idx, victim_way, entry));
        } else {
            req->m_status = CACHE_REQ_MISS;
            req->m_line_to_evict_copy = copy_cache_line(line);
            mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                      << " a request " << req->m_request_type 
                      << " on " << req->m_maddr << " could not evict a line (coherence protocol)" << endl;
        }
    }

    while (m_lines_to_invalidate.size()) {
        uint32_t idx = m_lines_to_invalidate.front().get<0>();
        uint32_t way = m_lines_to_invalidate.front().get<1>();
        shared_ptr<reqEntry>& entry = m_lines_to_invalidate.front().get<2>();
        shared_ptr<cacheRequest>& req = entry->request;

        /* evict line and return a copy */
        cacheLine &line = m_cache[idx][way];
        line.claimed = false;
        if (m_invalidate_hook) {
            (*m_invalidate_hook)(line, system_time);
        }

        if (req->m_do_claim) {
            line.claimed = true;
            line.ready = false;
            line.start_maddr = entry->start_maddr;
            if (m_claim_hook) {
                (*m_claim_hook)(line, system_time);
            }
        }

        req->m_status = CACHE_REQ_HIT;
        req->m_line_copy = copy_cache_line(line);

        m_lines_to_invalidate.erase(m_lines_to_invalidate.begin());
    }


    while (m_lines_to_evict.size()) {
        uint32_t idx = m_lines_to_evict.front().get<0>();
        uint32_t way = m_lines_to_evict.front().get<1>();
        shared_ptr<reqEntry>& entry = m_lines_to_evict.front().get<2>();
        shared_ptr<cacheRequest>& req = entry->request;

        /* evict line and return a copy */
        cacheLine &line = m_cache[idx][way];
        line.claimed = false;
        if (m_invalidate_hook) {
            (*m_invalidate_hook)(line, system_time);
        }
        req->m_line_to_evict_copy = copy_cache_line(line);

        if (req->m_do_claim) {
            line.claimed = true;
            line.ready = false;
            line.start_maddr = entry->start_maddr;
            if (m_claim_hook) {
                (*m_claim_hook)(line, system_time);
            }

            if (req->m_request_type == CACHE_REQ_UPDATE) {
                line.ready = true;
                line.data_dirty = false;
                line.coherence_info_dirty = false;
                line.last_access_time = system_time;
                if (req->m_word_count > 0) {
                    uint32_t offset = get_offset(req->m_maddr) / 4;
                    for (uint32_t i = 0; i < req->m_word_count; ++i) { 
                        line.data[offset + i] = req->m_data_to_write[i]; 
                    }
                    if (req->m_do_unset_dirty_on_write) {
                        line.data_dirty = false;
                    } else {
                        line.data_dirty = true; 
                    }
                }
                if (req->m_coherence_info_to_write) {
                    line.coherence_info = req->m_coherence_info_to_write;
                    if (req->m_do_unset_dirty_on_write) {
                        line.coherence_info_dirty = false;
                    } else {
                        line.coherence_info_dirty = true; 
                    }
                }

                if (m_update_hook) { (*m_update_hook)(line, system_time); }

                req->m_status = CACHE_REQ_HIT;
            } else {
                req->m_status = CACHE_REQ_MISS;
            }

            req->m_line_copy = copy_cache_line(line);
        } else {
            req->m_status = CACHE_REQ_MISS;
        }
        m_lines_to_evict.erase(m_lines_to_evict.begin());
    }

    /* advance hit testing */
    for (reqTable::iterator it_q = m_req_table.begin(); it_q != m_req_table.end(); ++it_q) {
        shared_ptr<reqEntry> &head = it_q->second.front();
        if (head->status == ENTRY_HIT_TEST) {
            if (--(head->remaining_hit_test_cycles) == 0) {
                head->status = ENTRY_DONE;
                m_ready_requests.push_back(head);
                mh_log(4) << "[cache " << m_id << " L" << m_level << " @ " << system_time << " ] "
                          << " a request " << head->request->m_request_type 
                          << " on " << head->request->m_maddr << " is in the ready queue "
                          << "(queue size " << m_ready_requests.size() << " )." << endl;
            }
        }
    }

}

