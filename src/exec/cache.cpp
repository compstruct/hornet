// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cache.hpp"
#include "stdio.h"

#define DEBUG

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X)
#define mh_log(X) LOG(log,X)
#endif

cacheRequest::cacheRequest(maddr_t maddr, cacheReqType_t request_type, uint32_t word_count,
                           shared_array<uint32_t> data_to_write, shared_ptr<void> coherence_info_to_write) :
    m_request_type(request_type), m_maddr(maddr), m_word_count(word_count),
    m_status(CACHE_REQ_NEW), m_line_copy(shared_ptr<cacheLine>()), 
    m_victim_line_copy(shared_ptr<cacheLine>()),
    m_coherence_info_to_write(coherence_info_to_write), m_data_to_write(data_to_write),
    m_do_clean_write(false), m_do_reserve(true), m_do_evict(true), m_stats_info(shared_ptr<void>())
{}

cacheRequest::~cacheRequest() {}

cache::cache(uint32_t level, uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r, 
             uint32_t words_per_line, uint32_t total_lines, uint32_t associativity, replacementPolicy_t replacement_policy,
             uint32_t hit_test_latency, uint32_t num_read_ports,  uint32_t num_write_ports) : 
    m_level(level), m_id(id), system_time(t), stats(st), log(l), ran(r), m_words_per_line(words_per_line), m_total_lines(total_lines),
    m_associativity(associativity), m_replacement_policy(replacement_policy), m_hit_test_latency(hit_test_latency),
    m_available_read_ports(num_read_ports), m_available_write_ports(num_write_ports),
    m_helper_copy_coherence_info(NULL), m_helper_is_hit(NULL), m_helper_reserve_line(NULL), m_helper_can_evict_line(NULL),
    m_helper_replacement_policy(NULL), m_helper_invalidate_hook(NULL)
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

// pretty printer
void cache::print_contents() {
    printf("\n[cache 0%d] Printing cache contents, level: %d\n", m_id, m_level);
    for (cacheTable::iterator it = m_cache.begin(); it != m_cache.end(); ++it) {
        uint32_t index = it->first;

        for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
            if (m_cache[index][it_way].valid) {
                printf("index = %d, way = %d: ", index, it_way);
                for (uint32_t o = 0; o < m_words_per_line; o++) {
                    printf("%x\t", m_cache[index][it_way].data[o]);        
                }
                printf("\n");
            }
        }
    }
    printf("\n");
}

shared_ptr<cacheLine> cache::copy_cache_line(const cacheLine &line) {
    shared_ptr<cacheLine> copy(new cacheLine);

    /* shallow copy */
    (*copy) = line;
    if (line.data) {
        copy->data = shared_array<uint32_t>(new uint32_t[m_words_per_line]);
        for (uint32_t i = 0; i < m_words_per_line; ++i) {
            copy->data[i] = line.data[i];
        }
    }
    
    /* deep copy */
    if (line.coherence_info) {
        mh_assert(m_helper_copy_coherence_info);
        copy->coherence_info = m_helper_copy_coherence_info(line.coherence_info);
    }

    return copy;
}

void cache::request(shared_ptr<cacheRequest> req) {

    reqEntry new_entry;
    new_entry.request = req;

    if (req->use_read_ports()) {
        mh_assert(read_port_available());
        --m_available_read_ports;
    } else {
        mh_assert(write_port_available());
        --m_available_write_ports;
    }
    if (m_hit_test_latency > 0) {
        new_entry.status = ENTRY_HIT_TEST;
        new_entry.remaining_hit_test_cycles = m_hit_test_latency;
    } else {
        new_entry.status = ENTRY_DONE;
    }

    new_entry.need_to_evict_and_reserve = false;

    req->m_status = CACHE_REQ_WAIT;
    maddr_t start_maddr = get_start_maddr_in_line(req->m_maddr);
    m_req_table[start_maddr].push_back(new_entry);

#if 0
    if (get_start_maddr_in_line(req->m_maddr).address == 0xdfef00) {
        cerr << " REQ " << req->m_maddr << " was requested @ " << system_time << endl;
    } 
#endif
    return;

}
    
void cache::tick_positive_edge() {

    /* writes and evictions are scheduled so writes always happen first*/
    set<maddr_t> written_lines; /* cannot evict a line that has been written in this cycle */

    for (reqTable::iterator it_q = m_req_table.begin(); it_q != m_req_table.end(); ) {
        reqQueue &q = it_q->second;
        reqEntry &head = q.front();;
        shared_ptr<cacheRequest> req = head.request;

        if (head.status == ENTRY_DONE) {

            //printf("Processing cache request @ level %d\n", m_level);
            //printf("Request space,address: %d,%x\n", (uint32_t) req->maddr().space, (uint32_t) req->maddr().address);
            //if (req->m_word_count == 1) 
            //    //printf("Request data 0: %x\n", req->m_data_to_write[0]);
            //if (req->m_word_count == 2)
            //    //printf("Request data 1: %x\n", req->m_data_to_write[1]);
            //if (req->m_word_count > 2) assert(false);

            uint32_t idx = get_index(req->m_maddr);
            maddr_t start_maddr = get_start_maddr_in_line(req->m_maddr);
            if (m_cache.count(idx) == 0) {
                m_cache[idx] = new cacheLine[m_associativity];
                for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                    m_cache[idx][it_way].empty = true;
                    m_cache[idx][it_way].valid = false;
                    m_cache[idx][it_way].data = shared_array<uint32_t>(new uint32_t[m_words_per_line]);
                    m_cache[idx][it_way].coherence_info = shared_ptr<void>();
                }
            }
            bool matched = false;
            vector<uint32_t> empties;
            for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                cacheLine &line = m_cache[idx][it_way];
                if (!line.empty && start_maddr == line.start_maddr) {
                    matched = true;

#if 0
                    if (start_maddr.address == 0xdfef00) {
                        cerr << " Testing " << start_maddr << " request for " << req->m_request_type << endl;
                        if (system_time > 26800) {
                            cerr << " hmm" << endl;
                        }
                    }
#endif

                    /* is it a real hit? */
                    bool hit = 
                        /* update is always a hit if matched. */
                        req->m_request_type == CACHE_REQ_UPDATE ||
                        /* invalidate also does not care either */
                        req->m_request_type == CACHE_REQ_INVALIDATE ||
                        (line.valid && /* need to be valid (otherwise, the line is still waiting for data or will be invalidated */
                               (!m_helper_is_hit ||  /* if it doesn't care coherency, it's a hit here */
                                !line.coherence_info ||  /* if it has no coherence information, it's regarded a hit too */
                                (*m_helper_is_hit)(req, line, system_time) /* otherwise the helper should say it's a hit */
                               ));

                    if (hit) {
                        //printf("Request was a HIT in the cache\n");

                        req->m_status = CACHE_REQ_HIT;
                        line.last_access_time = system_time;
                        switch (req->m_request_type) {
                        case CACHE_REQ_UPDATE:
                            //printf("\tRequest was an UPDATE\n");
#if 0
                            if (m_id == 62 && idx == 888) {
                                cerr << "  a line for " << start_maddr << " got into the cache 82 index 888 way " << it_way << " @ " << system_time << endl;
                            }
#endif
                            line.valid = true;
                            line.dirty = false;
                        case CACHE_REQ_WRITE:
                            {
                                //printf("\tRequest was an HIT\n");
                                //printf("\tWRITING TO THE CACHE, level: %d, index: %d\n", m_level, idx);
                                mh_assert(req->m_data_to_write);
                                
                                uint32_t offset = get_offset(req->m_maddr) / 4;
                                for (uint32_t i = 0; i < req->m_word_count; ++i) {
                                    line.data[offset + i] = req->m_data_to_write[i];
                                    //printf("\tWriting: %x\n", req->m_data_to_write[i]);
                                }
                                //printf("\n");
                                if (req->m_coherence_info_to_write) {
                                    line.coherence_info = req->m_coherence_info_to_write;
                                }
                                if (!req->m_do_clean_write) {
                                    line.dirty = true;
                                }
                                written_lines.insert(start_maddr);
                                req->m_line_copy = copy_cache_line(line);

                                //print_contents();

                                break;
                            }
                        case CACHE_REQ_INVALIDATE:
                            {
                                if (req->m_do_reserve) {
                                    req->m_line_copy = copy_cache_line(line);
                                    req->m_line_copy->start_maddr = start_maddr;
                                    req->m_line_copy->empty = false;
                                    req->m_line_copy->valid = false;
                                    if (m_helper_reserve_line) {
                                        (*m_helper_reserve_line)(*req->m_line_copy);
                                    }
                                    m_lines_to_evict_and_reserve.insert(make_tuple(idx, it_way, start_maddr));
                                } else {
                                    req->m_line_copy = copy_cache_line(line);
                                    req->m_line_copy->empty = true;
                                    req->m_line_copy->valid = false;
                                    m_lines_to_evict.insert(make_tuple(idx, it_way));
                                    if (m_helper_invalidate_hook) {
                                        (*m_helper_invalidate_hook)(line);
                                    }
                                }
                                break;
                            }
                        case CACHE_REQ_READ:
                            //printf("\t[cache 0%d] Request was an READ\n",  m_id);
                            req->m_line_copy = copy_cache_line(line);
                            break;
                        }
                        /* returns a copy of the updated line in any case (even for invalidate) */
                        break;
                    } else {
                        /* it's either invalid, or a coherence miss */
                        //printf("\t[cache 0%d] Request was INVALID or a COHERENCE MISS\n",  m_id);
                        req->m_status = CACHE_REQ_MISS;
                        line.last_access_time = system_time;
                        req->m_line_copy = copy_cache_line(line);
                        break;
                    }
                    /* doesn't reach here */
                } else if (line.empty) {
                    empties.push_back(it_way);
                }
            }
            if (!matched) {
                /* no entry */
                req->m_status = CACHE_REQ_MISS;
                mh_log(4) << "[cache " << m_id << " @ " << system_time << " ] a miss by mismatch for address "
                          << start_maddr << endl;
                if (req->m_do_reserve) {
                    if (!empties.empty()) {
                        /* reserve line */
                        /* Note : For now empty lines are reserved in a first-come first-serve way. */
                        /*        Depending on how requests to this cache are scheduled, it may create an dependency issue. */
                        /*        Currenty all memory classes perform a fair scheduling on this cache, so it's fine. */
                        /*        If you write a new memory class, keep this in mind. */
                        cacheLine &line = m_cache[idx][*empties.begin()];
                        line.start_maddr = start_maddr;
                        line.empty = false;
                        line.dirty = false;
                        line.valid = false;
                        if (m_helper_reserve_line) {
                            (*m_helper_reserve_line)(line);
                        }
                        req->m_line_copy = copy_cache_line(line);
                    } else {
                        head.need_to_evict_and_reserve = true;
                    }
                }
            }
            if (!head.need_to_evict_and_reserve) {
                /* exit code */
                if (req->use_read_ports()) {
                    ++m_available_read_ports;
                } else {
                    ++m_available_write_ports;
                }
                q.erase(q.begin());
                if (q.size() == 0) {
                    m_req_table.erase(it_q++);
                    continue;
                }
            }
            ++it_q;
        } else {
            ++it_q;
        }
    }

    for (reqTable::iterator it_q = m_req_table.begin(); it_q != m_req_table.end(); ) {
        reqQueue &q = it_q->second;
        reqEntry &head = q.front();;
        shared_ptr<cacheRequest> req = head.request;
        if (head.status == ENTRY_DONE) {
            /* assert(head.need_to_evict_and_reserve) */
            uint32_t idx = get_index(req->m_maddr);
            maddr_t start_maddr = get_start_maddr_in_line(req->m_maddr);
            vector<uint32_t> evictables;
            uint64_t lru_time = UINT64_MAX;
            uint32_t lru_way = 0;
            for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                cacheLine &line = m_cache[idx][it_way];
                if (line.valid && written_lines.count(line.start_maddr) == 0 && req->m_do_evict) {
                    evictables.push_back(it_way);
                    if (line.last_access_time < lru_time) {
                        lru_way = it_way;
                        lru_time = line.last_access_time;
                    }
                }
            }
            if (!evictables.empty()) {
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
                req->m_victim_line_copy = copy_cache_line(line);

                if (!m_helper_can_evict_line ||
                    (m_helper_can_evict_line && (*m_helper_can_evict_line)(line, system_time))) {

                    /* if we can evict right now */
                    req->m_line_copy = copy_cache_line(line);
                    req->m_line_copy->start_maddr = start_maddr;
                    req->m_line_copy->dirty = false;
                    req->m_line_copy->valid = false;
                    if (m_helper_reserve_line) {
                        (*m_helper_reserve_line)(*req->m_line_copy);
                    }
                    /* this line is reserved so returns it */
                    /* here's another case of first-come first-served arbitration (see above comments) */
                    m_lines_to_evict_and_reserve.insert(make_tuple(idx, victim_way, start_maddr));
                }
            }
            /* exit code */
            if (req->use_read_ports()) {
                ++m_available_read_ports;
            } else {
                ++m_available_write_ports;
            }
            q.erase(q.begin());
            if (q.size() == 0) {
                m_req_table.erase(it_q++);
                continue;
            }
            ++it_q;
        } else {
            /* not ready yet. proceed to the next entry */
            ++it_q;
        }
    }

}

void cache::tick_negative_edge() {

    for (set<tuple<uint32_t, uint32_t> >::iterator it = m_lines_to_evict.begin(); it != m_lines_to_evict.end(); ++it) {
        m_cache[it->get<0>()][it->get<1>()].empty = true;
        if (m_helper_invalidate_hook) {
            (*m_helper_invalidate_hook)(m_cache[it->get<0>()][it->get<1>()]);
        }
    }
    m_lines_to_evict.clear();

    for (set<tuple<uint32_t, uint32_t, maddr_t> >::iterator it = m_lines_to_evict_and_reserve.begin(); 
         it != m_lines_to_evict_and_reserve.end(); ++it) 
    {
        /* reserving case */
        cacheLine &line = m_cache[it->get<0>()][it->get<1>()];
        line.start_maddr = it->get<2>();
        line.empty = false;
        line.valid = false;
        if (m_helper_reserve_line) {
            (*m_helper_reserve_line)(line);
        }
    }
    m_lines_to_evict_and_reserve.clear();

    /* advance hit testing */
    for (reqTable::iterator it_q = m_req_table.begin(); it_q != m_req_table.end(); ++it_q) {
        reqEntry &head = it_q->second.front();
        if (head.status == ENTRY_HIT_TEST) {
            if (--(head.remaining_hit_test_cycles) == 0) {
                head.status = ENTRY_DONE;
            }
        }
    }

}

