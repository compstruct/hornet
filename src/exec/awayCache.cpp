// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "awayCache.hpp"
#include "error.hpp"

awayCache::awayCache(const uint32_t id, const uint32_t level, const uint64_t &t, 
             shared_ptr<tile_statistics> st, logger &log, shared_ptr<random_gen> ran,
             cache_cfg_t cfgs)
: cache(id, level, t, st, log, ran, cfgs) {}

awayCache::~awayCache() {
    map<uint64_t, cache_line_t*>::iterator i_idx;
    for (i_idx = m_cache.begin(); i_idx != m_cache.end(); ++i_idx) {
        for (uint32_t i_way = 0; i_way < m_cfgs.associativity; ++i_way) {
            delete[] (i_idx->second)[i_way].data;
        }
        delete[] i_idx->second;
    }
}

mreq_id_t awayCache::request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t target_level) {
    /* one cache line for one request */
    assert( req->addr()/m_cfgs.block_size_bytes == (req->addr() + req->byte_count() - 1)/m_cfgs.block_size_bytes);

    /* put an entry */
    /* assumes an infinite request table - if it's finite, deadlock must be considered */
    mreq_id_t new_id = take_new_mreq_id();
    in_req_entry_t new_entry;
    new_entry.status = REQ_INIT;
    new_entry.remaining_process_time = m_cfgs.process_time;
    new_entry.req = req;
    m_in_req_table[new_id] = new_entry;
    m_in_req_aux_info[new_id].location = location; 
    m_in_req_aux_info[new_id].target_level = target_level;

    /* initialize cache space if not allocated */
    uint64_t index = get_index(req->addr());
    if (m_cache.count(index) == 0) {
        m_cache[index] = new cache_line_t[m_cfgs.associativity];
        for (uint32_t i = 0; i < m_cfgs.associativity; ++i) {
            m_cache[index][i].valid = false;
            uint32_t* data = new uint32_t[m_cfgs.block_size_bytes/4];
            if (!data) {
                throw err_out_of_mem();
            }
            for (uint32_t j = 0; j < m_cfgs.block_size_bytes/4; ++j) {
                /* for now, initialize to zero */
                data[j] = 0;
            }
            m_cache[index][i].data = data;
        }
    }

    return new_id;
}

#ifdef WRITE_NOW
void awayCache::invalidate(maddr_t addr) {
    LOG(log,3) << "[awayCache " << m_id << " @ " << system_time << " ] is invalidating line " << hex << addr << dec << endl;
    cache_line_t* line = cache_line(addr);
    if (line) {
        line->timestamp = 0;
    }
}
#endif
bool awayCache::finish(mreq_id_t id) {
    if (m_in_req_table.count(id) == 0) {
        return true;
    } else if (m_in_req_table[id].status != REQ_DONE) {
        return false;
    } else {
        m_in_req_table.erase(id);
        m_in_req_aux_info.erase(id);
        return_mreq_id(id);
    }
    return true;
}

awayCache::cache_line_t* awayCache::cache_line(maddr_t addr) {
    uint64_t index = get_index(addr);
    uint64_t tag = get_tag(addr);
    assert(m_cache.count(index) > 0);
    for (uint32_t i = 0; i < m_cfgs.associativity; ++i) {
        if (m_cache[index][i].valid && m_cache[index][i].tag == tag) {
            return m_cache[index] + i;
        }
    }
    return NULL;
}

void awayCache::update() {
    /* update cache from home */
    vector<mreq_id_t> to_be_deleted;
    for (map<mreq_id_t, shared_ptr<memoryRequest> >::iterator i = m_out_req_table.begin(); i != m_out_req_table.end(); ++i) {
        mreq_id_t out_req_id = i->first;
        shared_ptr<memoryRequest> req = i->second;
        if (remote_memory()->ready(out_req_id)) {
            cache_line_t* line = cache_line(req->addr());   
            /* a line is waiting for data */
            line->ready = true;
            line->on_the_fly = false;
            if (req->rw() == MEM_READ) {   
                line->last_access = system_time;
                /* give new timestamp */
#ifdef WRITE_NOW
                if (line->timestamp == 0) {
                    /* this line is invalidated while the reply is coming */
                    line->timestamp = system_time;
                } else {
                    line->timestamp = UINT64_MAX;
                }
#else
                line->timestamp = req->timestamp();
#endif
                if (line->timestamp < system_time) {
                    /* only valid for this cycle */
                    line->timestamp = system_time;
                }
                /* a read copy arrived - put data on it */
                for (uint32_t j = 0; j < m_cfgs.block_size_bytes/4; ++j) {
                    (line->data)[j] = (req->data())[j];
                }
            }
            remote_memory()->finish(out_req_id);
            to_be_deleted.push_back(out_req_id);
        }
    }
    for (vector<mreq_id_t>::iterator i = to_be_deleted.begin(); i != to_be_deleted.end(); ++i) {
        m_out_req_table.erase(*i);
    }
}

void awayCache::process() {
    uint32_t transferred = 0;
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
        mreq_id_t in_req_id = i->first;
        /* processed by cache logic */
        if (i->second.status == REQ_BUSY) {
            assert(i->second.remaining_process_time > 0);
            --(i->second.remaining_process_time);
            if (i->second.remaining_process_time == 0) {
                i->second.status = REQ_WAIT;
            }
        }
        /* actions in cache hierarchy */
        if (i->second.status == REQ_WAIT) {
            shared_ptr<memoryRequest> req = i->second.req;
            cache_line_t* line = cache_line(req->addr());
            if (line) { 
                /* valid entry */
                if (line->ready) {
                    if (req->rw() == MEM_WRITE || line->timestamp < system_time) {
                        /* write requests are not cached */
                        line->valid = false;;
                        if (line->on_hold) {
                            i->second.status = REQ_DONE;
                        }
                    } else {
                        /* hit */
                        line->last_access = system_time;
                        if (transferred < m_cfgs.block_per_cycle) {
                            /* allowed bandwidth */
                            uint32_t *src, *tgt;
                            if (req->rw() == MEM_READ) {
                                src = line->data + get_offset(req->addr())/4;
                                tgt = req->data();
                                for (uint32_t j = 0; j < req->byte_count()/4; ++j) {
                                    tgt[j] = src[j];
                                }
                            }
                            ++transferred;
                            i->second.status = REQ_DONE;
                        }
                    }
                } else {
                    /* data is not here yet */
                    if (!line->on_the_fly) {
                        /* initiate read request - in case the line is evicted while waiting for the bandwidth */
                        shared_ptr<memoryRequest> home_req (new memoryRequest(req->tid(), req->addr() - req->addr()%m_cfgs.block_size_bytes,
                                    m_cfgs.block_size_bytes));
                        home_req->set_ra();
#ifdef WRITE_NOW
                        home_req->set_sender(m_id);
#endif
                        mreq_id_t new_id = remote_memory()->request(home_req, 
                                m_in_req_aux_info[in_req_id].location, m_in_req_aux_info[in_req_id].target_level);
                        if (stats->is_started()) {
                            stats->issue_remote_access();
                        }
                        m_out_req_table[new_id] = home_req;
                        line->on_the_fly = true;
                    }
                }
            } else {
                /* no entry - miss */
                bool has_space = false;
                vector<cache_line_t*> evict_cand;
                uint64_t index = get_index(req->addr());
                for (uint32_t i_way = 0; i_way < m_cfgs.associativity; ++i_way) {
                    if (m_cache[index][i_way].valid == false) {
                        /* empty space */
                        m_cache[index][i_way].valid = true;
                        m_cache[index][i_way].ready = false;
                        m_cache[index][i_way].tag = get_tag(req->addr());
                        m_cache[index][i_way].last_access = system_time;
#ifdef WRITE_NOW
                        m_cache[index][i_way].timestamp = system_time;
#endif
                        shared_ptr<memoryRequest> home_req;
                        if (req->rw() == MEM_READ) {
                            m_cache[index][i_way].on_hold = false;
                            home_req = shared_ptr<memoryRequest>(new memoryRequest(req->tid(), req->addr() - req->addr()%m_cfgs.block_size_bytes,
                                        m_cfgs.block_size_bytes));
                        } else {
                            m_cache[index][i_way].on_hold = true;
                            home_req = shared_ptr<memoryRequest>(new memoryRequest(req->tid(), req->addr(), req->byte_count()));
                        }
                        home_req->set_ra();
#ifdef WRITE_NOW
                        home_req->set_sender(m_id);
#endif
                        mreq_id_t new_id = remote_memory()->request(home_req, 
                                m_in_req_aux_info[in_req_id].location, m_in_req_aux_info[in_req_id].target_level);
                        if (stats->is_started()) {
                            stats->issue_remote_access();
                        }
                        m_out_req_table[new_id] = home_req;
                        m_cache[index][i_way].on_the_fly = true;
                        has_space = true;
                        break;
                    } else if (m_cache[index][i_way].ready && m_cache[index][i_way].last_access < system_time) {
                        evict_cand.push_back(m_cache[index] + i_way);
                    }
                    if (!has_space && !evict_cand.empty()) {
                        cache_line_t* tgt = NULL;
                        if (m_cfgs.policy == CACHE_RANDOM) {
                            tgt = evict_cand[ran->random_range(evict_cand.size())];
                        } else {
                            /* LRU */
                            for (vector<cache_line_t*>::iterator i_cand = evict_cand.begin(); i_cand != evict_cand.end(); ++i_cand) {
                                if (!tgt || (*i_cand)->last_access < tgt->last_access) {
                                    tgt = *i_cand;
                                }
                            }
                        }

                        /* LIBRARY COMPETITION */
                        /* Implement your own cache replacement policies here */

                        tgt->valid = false;
                    }
                }
            }
        }
    }
}


