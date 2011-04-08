// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cache.hpp"

static bool maddr_equal(maddr_t a, maddr_t b) {
    return (a.mem_space_id == b.mem_space_id) && (a.address == b.address);
}

cacheRequest::cacheRequest(maddr_t maddr, cacheReqType_t request_type, shared_ptr<void> info_to_set) : 
    m_request_type(request_type), m_maddr(maddr), 
    m_status(CACHE_NEW), m_cache_line(NULL), m_need_to_invalidate(false), m_coherence_info_to_set(info_to_set) {}

cacheRequest::~cacheRequest() {}


cache::cache(uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r, 
             uint32_t words_per_line, uint32_t total_lines, uint32_t associativity, replacementPolicy_t replacement_policy,
             uint32_t hit_test_latency, uint32_t num_read_ports, uint32_t num_write_ports) : 
    m_id(id), system_time(t), stats(st), log(l), ran(r), m_words_per_line(words_per_line), m_total_lines(total_lines),
    m_associativity(associativity), m_replacement_policy(replacement_policy), m_hit_test_latency(hit_test_latency),
    m_number_of_free_read_ports(num_read_ports), m_number_of_free_write_ports(num_write_ports)
{ 
    int log2 = 0;
    for (uint32_t i = m_words_per_line * 4; i > 1; i = i/2) {
        assert(i%2 == 0); /*  must be a power of 2 */
        ++log2;
    }
    m_offset_mask = m_words_per_line - 1;
    m_index_pos = log2;
    m_index_mask = (m_total_lines / m_associativity - 1) << m_index_pos;
}

cache::~cache() {
    map<uint32_t, cacheLine_t*>::iterator it_idx;
    for (it_idx = m_cache.begin(); it_idx != m_cache.end(); ++it_idx) {
        delete[] it_idx->second;
    }
}

void cache::request(shared_ptr<cacheRequest> req) {
    req->m_status = CACHE_WAIT;
    shared_ptr<entry_t> new_entry = shared_ptr<entry_t>(new entry_t);
    new_entry->status = ENTRY_PORT;
    new_entry->request = req;
    if (req->m_request_type == CACHE_REQ_WRITE || req->m_request_type == CACHE_REQ_ADD_LINE) {
        if (m_number_of_free_write_ports > 0) {
            --m_number_of_free_write_ports;
            if (m_hit_test_latency > 0) {
                new_entry->status = ENTRY_HIT_TEST;
                new_entry->remaining_hit_test_cycles = m_hit_test_latency;
            } else {
                new_entry->status = ENTRY_DONE;
            }
            maddr_t first_maddr = get_first_maddr(req->m_maddr);
            m_entry_tables[first_maddr.mem_space_id][first_maddr.address].push_back(new_entry);
        } else {
            m_entries_waiting_for_write_ports.push_back(new_entry);
        }
    } else {
        if (m_number_of_free_read_ports > 0) {
            --m_number_of_free_read_ports;
            new_entry->status = ENTRY_HIT_TEST;
            if (m_hit_test_latency > 0) {
                new_entry->status = ENTRY_HIT_TEST;
                new_entry->remaining_hit_test_cycles = m_hit_test_latency;
            } else {
                new_entry->status = ENTRY_DONE;
            }
            maddr_t first_maddr = get_first_maddr(req->m_maddr);
            m_entry_tables[first_maddr.mem_space_id][first_maddr.address].push_back(new_entry);
        } else {
            m_entries_waiting_for_read_ports.push_back(new_entry);
        }
    }
}
    
void cache::tick_positive_edge() {

    map<uint32_t, entryTable>::iterator it_space;
    for (it_space = m_entry_tables.begin(); it_space != m_entry_tables.end(); ++it_space) {
        uint32_t current_space __attribute__((unused)) = it_space->first;
        entryTable &current_table = it_space->second;
        entryTable::iterator it_addr;
        for (it_addr = current_table.begin(); it_addr != current_table.end(); ++it_addr) {
            uint64_t current_addr __attribute__((unused)) = it_addr->first;
            entryQueue &current_queue = it_addr->second;
            /* NOTE : Only one request it served for the same line (Do NOT change this behavior) */
            shared_ptr<entry_t> current_entry = current_queue.front();
            shared_ptr<cacheRequest> current_request = current_entry->request;
            if (current_entry->status == ENTRY_DONE) {
                /* when hit testing is done */
                uint32_t index = get_index(current_request->m_maddr);
                maddr_t first_maddr = get_first_maddr(current_request->m_maddr);
                if (m_cache.count(index) == 0) {
                    /* prepare lines for the given index if not ready */
                    m_cache[index] = new cacheLine_t[m_associativity];
                    for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                        m_cache[index][it_way].valid = false;
                    }
                } 
                bool hit = false;
                bool has_empty_line = false;
                uint32_t any_empty_way = 0;

                vector<uint32_t/*way*/> evict_candidates;
                uint64_t lru = UINT64_MAX;
                uint32_t lru_way = 0;

                for (uint32_t it_way = 0; it_way < m_associativity; ++it_way) {
                    cacheLine_t &line = m_cache[index][it_way];
                    if (!line.valid) {
                        if (!line.reserved) {
                            /* empty line */
                            has_empty_line = true;
                            any_empty_way = it_way;
                        }
                    } else if (maddr_equal(first_maddr, line.first_maddr)) {
                        /* cache hit */
                        current_request->m_status = CACHE_HIT;
                        current_request->m_cache_line = &line;
                        line.last_access_time = system_time;
                        if (current_request->m_request_type  == CACHE_REQ_INVALIDATE) {
                            linePosition_t pos = {index, it_way};
                            m_lines_to_purge.push_back(pos);
                        } else if (current_request->m_request_type == CACHE_REQ_WRITE) {
                            line.dirty = true;
                        } else if (current_request->m_request_type == CACHE_REQ_ADD_LINE) {
                            /* ADD_LINE resets dirty & coherence states */
                            line.dirty = false;
                            line.coherence_info = current_request->m_coherence_info_to_set;
                        }
                        hit = true;
                        break;
                    } else if (!has_empty_line && line.last_access_time < lru) {
                        evict_candidates.push_back(it_way);
                        lru = line.last_access_time;
                        lru_way = it_way;
                    }
                }
                if (!hit) {
                    uint32_t way_to_evict = 0;
                    if (!evict_candidates.empty()) {
                        switch (m_replacement_policy) {
                        case REPLACE_RANDOM:
                            way_to_evict = evict_candidates[ran->random_range(evict_candidates.size())];
                            break;
                        case REPLACE_LRU:
                            way_to_evict = lru_way;
                            break;
                        } 
                    }
                    switch(current_request->m_request_type) {
                    case CACHE_REQ_READ:
                    case CACHE_REQ_WRITE:
                        if (!has_empty_line) {
                            if (!evict_candidates.empty()) {
                                current_request->m_status = CACHE_MISS;
                                current_request->m_need_to_invalidate = true;
                                current_request->m_maddr_to_invalidate = m_cache[index][way_to_evict].first_maddr;
                                m_cache[index][way_to_evict].reserved = true;
                            } else {
                                continue; /* wait for another cycle to find a space */
                            }
                        } else {
                            current_request->m_status = CACHE_MISS;
                            m_cache[index][any_empty_way].reserved = true;
                        }
                        break;
                    case CACHE_REQ_ADD_LINE:
                        if (has_empty_line) {
                            current_request->m_status = CACHE_HIT;
                            m_cache[index][any_empty_way].valid = true;
                            m_cache[index][any_empty_way].dirty = false;
                            m_cache[index][any_empty_way].reserved = false;
                            m_cache[index][any_empty_way].last_access_time = system_time;
                            m_cache[index][any_empty_way].first_maddr = first_maddr;
                            m_cache[index][any_empty_way].data = shared_array<uint32_t>(new uint32_t[m_words_per_line]);
                            m_cache[index][any_empty_way].coherence_info = current_request->m_coherence_info_to_set;
                        } else {
                            if (!evict_candidates.empty()) {
                                current_request->m_status = CACHE_MISS;
                                current_request->m_need_to_invalidate = true;
                                current_request->m_maddr_to_invalidate = m_cache[index][way_to_evict].first_maddr;
                                m_cache[index][way_to_evict].reserved = true;
                            } else {
                                continue; /* wait for another cycle to find a space */
                            }
                        }
                        break;
                    case CACHE_REQ_INVALIDATE:
                        current_request->m_status = CACHE_MISS;
                        break;
                    }
                }
                /* release the port */
                if (current_request->m_request_type == CACHE_REQ_WRITE || current_request->m_request_type == CACHE_REQ_ADD_LINE) {
                    ++m_number_of_free_write_ports;
                } else {
                    ++m_number_of_free_read_ports;
                }
                /* remove from the queue */
                current_queue.erase(current_queue.begin());
            }
        }
    }

}

void cache::tick_negative_edge() {

    /* advance hit testing */
    map<uint32_t, entryTable>::iterator it_space;
    for (it_space = m_entry_tables.begin(); it_space != m_entry_tables.end(); ++it_space) {
        uint32_t current_space __attribute__((unused)) = it_space->first;
        entryTable &current_table = it_space->second;
        entryTable::iterator it_addr;
        for (it_addr = current_table.begin(); it_addr != current_table.end(); ++it_addr) {
            uint64_t current_addr __attribute__((unused)) = it_addr->first;
            entryQueue &current_queue = it_addr->second;
            shared_ptr<entry_t> current_entry = current_queue.front();
            if (current_entry->status == ENTRY_HIT_TEST) {
                if (--(current_entry->remaining_hit_test_cycles) == 0) {
                    current_entry->status = ENTRY_DONE;
                }
            }
        }
    }

    /* read port serialization */
    while(m_number_of_free_read_ports > 0 && !m_entries_waiting_for_read_ports.empty()) {
        shared_ptr<entry_t> entry = m_entries_waiting_for_read_ports.front();
        --m_number_of_free_read_ports;
        entry->status = ENTRY_HIT_TEST;
        if (m_hit_test_latency > 0) {
            entry->status = ENTRY_HIT_TEST;
            entry->remaining_hit_test_cycles = m_hit_test_latency;
        } else {
            entry->status = ENTRY_DONE;
        }
        maddr_t first_maddr = get_first_maddr(entry->request->m_maddr);
        m_entry_tables[first_maddr.mem_space_id][first_maddr.address].push_back(entry);
        m_entries_waiting_for_read_ports.erase(m_entries_waiting_for_read_ports.begin());
    }

    /* write port serialization */
    while(m_number_of_free_write_ports > 0 && !m_entries_waiting_for_write_ports.empty()) {
        shared_ptr<entry_t> entry = m_entries_waiting_for_write_ports.front();
        --m_number_of_free_write_ports;
        if (m_hit_test_latency > 0) {
            entry->status = ENTRY_HIT_TEST;
            entry->remaining_hit_test_cycles = m_hit_test_latency;
        } else {
            entry->status = ENTRY_DONE;
        }
        maddr_t first_maddr = get_first_maddr(entry->request->m_maddr);
        m_entry_tables[first_maddr.mem_space_id][first_maddr.address].push_back(entry);
        m_entries_waiting_for_write_ports.erase(m_entries_waiting_for_write_ports.begin());
    }

    /* purge zombies */
    vector<linePosition_t>::iterator it;
    for (it = m_lines_to_purge.begin(); it != m_lines_to_purge.end(); ++it) {
        m_cache[it->index][it->way].valid = false;
    }
    m_lines_to_purge.clear();

}


