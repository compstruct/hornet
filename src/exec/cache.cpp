// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cache.hpp"

#define DEBUG

#ifdef DEBUG
#define mh_cerr cerr
#else
#define mh_cerr LOG(log,4)
#endif

cacheRequest::cacheRequest(maddr_t maddr, cacheReqType_t request_type, uint32_t word_count,
                           shared_array<uint32_t> data_to_write, shared_ptr<void> coherence_info_to_write) :
    m_request_type(request_type), m_maddr(maddr), m_word_count(word_count),
    m_status(CACHE_REQ_NEW), m_line_copy(shared_ptr<cacheLine>()), 
    m_victim_line_copy(shared_ptr<cacheLine>()),
    m_coherence_info_to_write(coherence_info_to_write), m_data_to_write(data_to_write) 
{}

cacheRequest::~cacheRequest() {}

cache::cache(uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r, 
             uint32_t words_per_line, uint32_t total_lines, uint32_t associativity, replacementPolicy_t replacement_policy,
             uint32_t hit_test_latency, uint32_t num_read_ports,  uint32_t num_write_ports) : 
    m_id(id), system_time(t), stats(st), log(l), ran(r), m_words_per_line(words_per_line), m_total_lines(total_lines),
    m_associativity(associativity), m_replacement_policy(replacement_policy), m_hit_test_latency(hit_test_latency),
    m_available_read_ports(num_read_ports), m_available_write_ports(num_write_ports),
    m_helper_copy_coherence_info(NULL), m_helper_will_return_line(NULL), m_helper_is_hit(NULL), m_helper_reserve_empty_line(NULL),
    m_helper_can_evict_line(NULL)
{ 
    assert(m_associativity > 0);
    assert(m_words_per_line > 0);
    assert(m_available_read_ports > 0);
    assert(m_available_write_ports > 0);
    int log2 = 0;
    for (uint32_t i = m_words_per_line * 4; i > 1; i = i/2) {
        assert(i%2 == 0); /*  must be a power of 2 */
        ++log2;
    }
    m_offset_mask = m_words_per_line - 1;
    m_index_pos = log2;
    m_index_mask = (m_total_lines / m_associativity - 1) << m_index_pos;
}

cache::~cache() {}

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
        assert(m_helper_copy_coherence_info);
        copy->coherence_info = m_helper_copy_coherence_info(line.coherence_info);
    }

    return copy;
}

void cache::request(shared_ptr<cacheRequest> req) {

    reqEntry new_entry;
    new_entry.request = req;

    switch (req->m_request_type) {
    case CACHE_REQ_READ:
    case CACHE_REQ_COHERENCE_READ:
    case CACHE_REQ_INVALIDATE:
    case CACHE_REQ_COHERENCE_INVALIDATE:
        assert(read_port_available());
        --m_available_read_ports;
        break;
    case CACHE_REQ_WRITE:
    case CACHE_REQ_COHERENCE_WRITE:
        assert(write_port_available());
        --m_available_write_ports;
        break;
    }
    if (m_hit_test_latency > 0) {
        new_entry.status = ENTRY_HIT_TEST;
        new_entry.remaining_hit_test_cycles = m_hit_test_latency;
    } else {
        new_entry.status = ENTRY_DONE;
    }

    maddr_t start_maddr = get_start_maddr_in_line(req->m_maddr);
    m_req_table[start_maddr].push_back(new_entry);

    return;

}
    
void cache::tick_positive_edge() {
    for (reqTable::iterator it_q = m_req_table.begin(); it_q != m_req_table.end(); ++it_q) {
        reqQueue &q = it_q->second;
        reqEntry &head = q.front();;
        shared_ptr<cacheRequest> req = head.request;
        if (head.status == ENTRY_DONE) {
            uint32_t idx = get_index(req->m_maddr);
            maddr_t start_maddr = get_start_maddr_in_line(req->m_maddr);
            if (m_cache.count(idx) == 0) {
                m_cache[idx] = new cacheLine[m_associativity];
                for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                    m_cache[idx][it_way].empty = true;
                }
            }
            vector<uint32_t> evictables;
            vector<uint32_t> empties;
            uint64_t lru_time = UINT64_MAX;
            uint32_t lru_way = 0;
            for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                cacheLine &line = m_cache[idx][it_way];
                if (!line.empty && start_maddr == line.start_maddr) {

                    bool coherence_hit = 
                        /* either it's not a coherence access */
                        req->m_request_type == CACHE_REQ_READ ||
                        req->m_request_type == CACHE_REQ_WRITE ||
                        req->m_request_type == CACHE_REQ_INVALIDATE ||
                        /* or it doesn't have a helper */
                        !m_helper_is_hit ||
                        /* or the cache line doesn't have a coherence info */
                        !line.coherence_info ||
                        /* or the helper says it's a hit */
                        (*m_helper_is_hit)(req,line);

                    if (coherence_hit) {
                        req->m_status = CACHE_REQ_HIT;
                        switch (req->m_request_type) {
                        case CACHE_REQ_WRITE:
                        case CACHE_REQ_COHERENCE_WRITE:
                            {
                                assert(!req->m_data_to_write);
                                uint32_t offset = get_offset(req->m_maddr);
                                for (uint32_t i = 0; i < req->m_word_count; ++i) {
                                    line.data[offset + i] = req->m_data_to_write[i];
                                }
                                if (req->m_coherence_info_to_write) {
                                    line.coherence_info = req->m_coherence_info_to_write;
                                }
                                line.dirty = true;
                                ++m_available_write_ports;
                                break;
                            }
                        case CACHE_REQ_INVALIDATE:
                        case CACHE_REQ_COHERENCE_INVALIDATE:
                            {
                                maddr_t not_used = { 0, 0 };
                                m_lines_to_invalidate.insert(make_tuple(idx, it_way, false, not_used));
                                ++m_available_read_ports;
                                break;
                            }
                        case CACHE_REQ_READ:
                        case CACHE_REQ_COHERENCE_READ:
                            ++m_available_read_ports;
                            /* nothing to do with read - we are returning the whole line */
                            break;
                        }
                    } else {
                        req->m_status = CACHE_REQ_MISS;
                    }


                    line.last_access_time = system_time;
                    if (m_helper_will_return_line) {
                        (*m_helper_will_return_line)(line);
                    }

                    req->m_line_copy = copy_cache_line(line);

                    break;

                } else if (line.empty) {
                    empties.push_back(it_way);
                } else {
                    evictables.push_back(it_way);
                    if (line.last_access_time < lru_time) {
                        lru_time = line.last_access_time;
                        lru_way = it_way;
                    }
                }
            }
            if (!req->m_line_copy) {
                /* no entry */
                if (req->m_request_type == CACHE_REQ_INVALIDATE || req->m_request_type == CACHE_REQ_COHERENCE_INVALIDATE) {
                    req->m_status = CACHE_REQ_HIT;
                } else if (!empties.empty()) {
                    req->m_status = CACHE_REQ_MISS;
                    if (m_helper_reserve_empty_line) {
                        cacheLine &line = m_cache[idx][*empties.begin()];
                        line.start_maddr = start_maddr;
                        /* Here's a somewhat non-combinational fifo arbitration, but it's okay becaues requests would be */
                        /* randomly serialized... */
                        line.empty = false;
                        (*m_helper_reserve_empty_line)(line);
                    }
                } else {
                    req->m_status = CACHE_REQ_MISS;
                    uint32_t victim_way = 0;
                    switch (m_replacement_policy) {
                    case REPLACE_RANDOM:
                        victim_way = evictables[ran->random_range(evictables.size())];
                        break;
                    case REPLACE_LRU:
                        victim_way = lru_way;
                        break;
                    }
                    cacheLine &line = m_cache[idx][victim_way];

                    if (!m_helper_can_evict_line || !line.coherence_info || !(*m_helper_can_evict_line)(line)) {
                        m_lines_to_invalidate.insert(make_tuple(idx, victim_way, true, start_maddr));
                    }
                    if (m_helper_will_return_line) {
                        (*m_helper_will_return_line)(m_cache[idx][victim_way]);
                    }
                    req->m_victim_line_copy = copy_cache_line(m_cache[idx][victim_way]);
                }       
                switch (req->m_request_type) {
                case CACHE_REQ_WRITE:
                case CACHE_REQ_COHERENCE_WRITE:
                    ++m_available_write_ports;
                    break;
                case CACHE_REQ_INVALIDATE:
                case CACHE_REQ_COHERENCE_INVALIDATE:
                    ++m_available_read_ports;
                    break;
                case CACHE_REQ_READ:
                case CACHE_REQ_COHERENCE_READ:
                    ++m_available_read_ports;
                    break;
                }

            }
            q.erase(q.begin());
            if (q.size() == 0) {
                m_req_table.erase(it_q);
            }
        }
    }

}

void cache::tick_negative_edge() {

    for (set<tuple<uint32_t, uint32_t, bool, maddr_t> >::iterator it = m_lines_to_invalidate.begin(); 
         it != m_lines_to_invalidate.end(); ++it) 
    {
        if (it->get<2>()) {
            /* reserving case */
            cacheLine &line = m_cache[it->get<0>()][it->get<1>()];
            line.start_maddr = it->get<3>();
            if (!m_helper_reserve_empty_line) {
                (*m_helper_reserve_empty_line)(line);
            }
        } else {
            /* invalidate only case */
            m_cache[it->get<0>()][it->get<1>()].empty = true;
        }
    }
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
