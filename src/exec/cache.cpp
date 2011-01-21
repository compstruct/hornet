// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cache.hpp"

cache::cache(const uint32_t id, const uint64_t &t, 
             logger &log, shared_ptr<random_gen> ran,
             cache_cfg_t cfgs)
: memory(id, t, log, ran), m_cfgs(cfgs), m_home(shared_ptr<cache>()) {
    int log2 = 0;
    for (uint32_t i = m_cfgs.block_size_bytes*8; i > 1; i = i/2) {
        assert(i%2 == 0); /* must be a power of 2 */
        ++log2;
    }
    m_offset_mask = m_cfgs.block_size_bytes * 8 - 1;
    m_index_pos = log2;

    log2 = 0;
    assert(m_cfgs.total_block % m_cfgs.associativity == 0);
    for (uint32_t i = m_cfgs.total_block / m_cfgs.associativity; i > 1; i = i/2) {
        assert(i%2 == 0); /* must be a power of 2 */
        ++log2;
    }
    m_index_mask = (m_cfgs.total_block / m_cfgs.associativity - 1) << m_index_pos;
    m_tag_pos = log2 + m_index_pos;

    m_tag_mask = ~(m_index_mask | m_offset_mask);

}

cache::~cache() {}

void cache::set_home(shared_ptr<memory> home) {
    m_home = home;
}

mreq_id_t cache::request(shared_ptr<memoryRequest> req) {
    /* one cache line for one request */
    assert( req->addr()/m_cfgs.block_size_bytes == (req->addr() + req->byte_count() - 1)/m_cfgs.block_size_bytes);

    /* put an entry */
    mreq_id_t new_id = take_new_mreq_id();
    in_req_entry_t new_entry;
    new_entry.status = REQ_INIT;
    new_entry.remaining_process_time = m_cfgs.process_time;
    new_entry.req = req;
    m_in_req_table[new_id] = new_entry;

    /* initialize cache space if not allocated */
    uint64_t index = get_index(req->addr());
    if (m_cache.count(index) == 0) {
        shared_ptr<vector<shared_ptr<cache_line_t> > > new_index(new vector<shared_ptr<cache_line_t> >);
        m_cache[index] = new_index;
        for (uint32_t j = 0; j < m_cfgs.associativity; ++j) {
            shared_ptr<cache_line_t> new_line (new cache_line_t());
            new_line->valid = false;
            shared_ptr<uint32_t> data (new uint32_t(m_cfgs.block_size_bytes/4));
            new_line->data = data;
            m_cache[index]->push_back(new_line);
        }
    }

    return new_id;
}

bool cache::ready(mreq_id_t id) {
    return (m_in_req_table.count(id) > 0 && m_in_req_table[id].status == REQ_DONE);
}

bool cache::finish(mreq_id_t id) {
    if (m_in_req_table.count(id) == 0) {
        return true;
    } else if (m_in_req_table[id].status != REQ_DONE) {
        return false;
    } else {
        m_in_req_table.erase(id);
        return_mreq_id(id);
    }
    return true;
}

void cache::initiate() {
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
        if (i->second.status == REQ_INIT) {
            i->second.status = REQ_BUSY;
        }
    }
}

#if 0
bool cache::has_line(maddr_t addr) {
    shared_ptr<cache_line_t> line = cache_line(addr);
    return (line != shared_ptr<cache_line_t>() && line->valid && !line->doomed);
}

bool cache::hit(maddr_t addr) {
    shared_ptr<cache_line_t> line = cache_line(addr);
    return (has_line(addr) && line->ready);
}
#endif

shared_ptr<cache::cache_line_t> cache::cache_line(maddr_t addr) {
    uint64_t index = get_index(addr);
    uint64_t tag = get_tag(addr);
    assert(m_cache.count(index) > 0);
    for (vector<shared_ptr<cache_line_t> >::iterator l = m_cache[index]->begin(); l != m_cache[index]->end(); ++l) {
        if ((*l)->valid && (*l)->tag == tag) {
            return *l;
        }
    }
    return shared_ptr<cache_line_t>();
}

void cache::update() {
    /* update cache from home */
    for (map<mreq_id_t, shared_ptr<memoryRequest> >::iterator i = m_out_req_table.begin(); i != m_out_req_table.end(); ++i) {
        mreq_id_t req_id = i->first;
        shared_ptr<memoryRequest> req = i->second;
        if (m_home->ready(req_id)) {
            shared_ptr<cache_line_t> line = cache_line(req->addr());   
            if (req->rw() == MEM_READ) {   
                /* a read copy arrived - line must be in invaid, on_the_fly status */
                line->ready = true;
                line->on_the_fly = false;
                line->last_access = system_time;
                for (uint32_t j = 0; j < m_cfgs.block_size_bytes/4; ++j) {
                    (line->data.get())[j] = (req->data().get())[j];
                }
            } else {
                /* write back is acknowledged  - line must be in doomed status */
                line->valid = false;
            }
        }
    }
}

void cache::process() {
    uint32_t transferred = 0;
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
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
            shared_ptr<cache_line_t> line = cache_line(req->addr());
            if (line == shared_ptr<cache_line_t>()) {
                /* no entry - miss */
                bool has_space = false;
                vector<shared_ptr<cache_line_t> > evict_cand;
                uint64_t index = get_index(req->addr());
                for (vector<shared_ptr<cache_line_t> >::iterator j = m_cache[index]->begin(); j != m_cache[index]->end(); ++j) {
                    if ((*j)->valid == false) {
                        /* empty space */
                        (*j)->valid = true;
                        (*j)->ready = false;
                        (*j)->doomed = false;
                        (*j)->tag = get_tag(req->addr());
                        (*j)->dirty = false;
                        (*j)->last_access = system_time;
                        shared_ptr<memoryRequest> home_req (new memoryRequest(MEM_READ,
                                    req->addr()/m_cfgs.block_size_bytes,
                                    (*j)->data,
                                    m_cfgs.block_size_bytes));
                        mreq_id_t new_id = m_home->request(home_req);
                        m_out_req_table[new_id] = home_req;
                        (*j)->on_the_fly = true;
                        has_space = true;
                        break;
                    } else if ((*j)->doomed) {
                        has_space = true;
                    } else if ((*j)->ready) {
                        evict_cand.push_back(*j);
                    }
                }
                if (!has_space && !evict_cand.empty()) {
                    shared_ptr<cache_line_t> tgt = shared_ptr<cache_line_t>();
                    if (m_cfgs.policy == CACHE_RANDOM) {
                        tgt = evict_cand[ran->random_range(evict_cand.size())];
                    } else {
                        /* LRU */
                        for (vector<shared_ptr<cache_line_t> >::iterator j = evict_cand.begin(); j != evict_cand.end(); ++j) {
                            if (tgt == shared_ptr<cache_line_t>() || (*j)->last_access < tgt->last_access) {
                                tgt = *j;
                            }
                        }
                    }
                }
            } else if (!line->doomed) {  /* if doomed, just wait until it gets an ack */
                /* valid entry */
                if (line->ready) {
                    /* hit */
                    if (transferred < m_cfgs.block_per_cycle) {
                        /* allowed bandwidth */
                        uint32_t *src, *tgt;
                        if (req->rw() == MEM_READ) {
                            src = line->data.get() + get_offset(req->addr());;
                            tgt = req->data().get();
                        } else {
                            src = req->data().get();
                            tgt = line->data.get() + get_offset(req->addr());;
                            line->dirty = true;
                        }
                        for (uint32_t j = 0; j < req->byte_count()/4; ++j) {
                            tgt[j] = src[j];
                        }
                        ++transferred;
                        i->second.status = REQ_DONE;
                    }
                } else {
                    /* data is not here yet */
                    if (!line->on_the_fly) {
                        /* initiate read request - in case the line is evicted while waiting for the bandwidth */
                        shared_ptr<memoryRequest> home_req (new memoryRequest(MEM_READ,
                                    req->addr()/m_cfgs.block_size_bytes,
                                    line->data,
                                    m_cfgs.block_size_bytes));
                        mreq_id_t new_id = m_home->request(home_req);
                        m_out_req_table[new_id] = home_req;
                        line->on_the_fly = true;
                    }
                }
            }
        }
#if 0

        if (i->second.status == REQ_WAIT || i->second.status == REQ_FLY ) {
            shared_ptr<memoryRequest> req = i->second.req;
            if (hit(req->addr()) && transferred < m_cfgs.block_per_cycle) {
                cache_line_t line = cache_line(req->addr());
                uint32_t *src, *tgt;
                if (req->rw() == MEM_READ) {
                    src = line.data.get() + get_offset(req->addr());;
                    tgt = req->data().get();
                } else {
                    src = req->data().get();
                    tgt = line.data.get() + get_offset(req->addr());;
                    line.dirty = true;
                }
                for (uint32_t j = 0; j < req->byte_count()/4; ++j) {
                    tgt[j] = src[j];
                }
                ++transferred;
                i->second.status = REQ_DONE;
            }
        }
        /* if it misses and the corresponding line is not yet requested to home, */
        if (i->second.status == REQ_WAIT && 
        if (i->second.status == REQ_WAIT || i->second.status == REQ_FLY) {
            shared_ptr<memoryRequest> req = i->second.req;
            if (hit(req->addr()) && transferred < m_cfgs.block_per_cycle) {
                cache_line_t line = cache_line(req->addr());
                uint32_t *src, *tgt;
                if (req->rw() == MEM_READ) {
                    src = line.data.get() + get_offset(req->addr());;
                    tgt = req->data().get();
                } else {
                    src = req->data().get();
                    tgt = line.data.get() + get_offset(req->addr());;
                    line.dirty = true;
                }
                for (uint32_t j = 0; j < req->byte_count()/4; ++j) {
                    tgt[j] = src[j];
                }
                ++transferred;
                i->second.status = REQ_DONE;
            } else if (i->second.status == REQ_WAIT) {
                uint64_t index = get_index(req->addr());
                if (m_cache[index]->size() < m_associativity - 1) {
                    shared_ptr<uint32_t> buffer (new uint32_t (m_cfgs.block_size_bytes/4));
                    shared_ptr<memoryRequest> home_req (new memoryRequest(MEM_READ,
                                req->addr()/m_cfgs.block_size_bytes,
                                buffer,
                                m_cfgs.block_size_bytes));
                    mreq_id_t new_id = m_home->request(home_req);
                    m_out_req_table[new_id] = home_req;
                    req->status = REQ_FLY;
                } else {
                    if (m_cfgs.policy == CACHE_RANDOM) {

                    } else {
                        /* LRU */
                    }
                }
            }
        }
#endif
    }
    cerr << "process end" << endl;
}


