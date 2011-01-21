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

    mreq_id_t new_id = take_new_mreq_id();
    in_req_entry_t new_entry;
    new_entry.status = REQ_INIT;
    new_entry.remaining_process_time = m_cfgs.process_time;
    new_entry.req = req;
    m_in_req_table[new_id] = new_entry;
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
    m_transferred_this_cycle = 0;
}

bool cache::hit(maddr_t addr) {
    return (cache_line(addr).data != shared_ptr<uint32_t>());
}

cache::cache_line_t cache::cache_line(maddr_t addr) {
    cache_line_t ret;
    ret.data = shared_ptr<uint32_t>();
    uint64_t index = get_index(addr);
    uint64_t tag = get_tag(addr);
    for (vector<cache_line_t>::iterator l = m_cache[index]->begin(); l != m_cache[index]->end(); ++l) {
        if ((*l).tag == tag) {
            ret = *l;
        }
    }
    return ret;
}

void cache::update() {
    /* update cache from home */
    for (map<mreq_id_t, shared_ptr<memoryRequest> >::iterator i = m_out_req_table.begin(); i != m_out_req_table.end(); ++i) {
        mreq_id_t req_id = i->first;
        shared_ptr<memoryRequest> req = i->second;
        if (m_home->ready(req_id)) {
            // replace if needed (do not twice)
            // update cache entry
            m_home->finish(req_id);
        }
    }
    /* update in_req_table */
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
        if (i->second.status == REQ_WAIT) {
            maddr_t addr = i->second.req->addr();
            if (hit(addr) && m_transferred_this_cycle < m_cfgs.block_per_cycle) {
                cache_line_t line = cache_line(addr);
                for (uint32_t j = 0; j < i->second.req->byte_count()/4; ++j) {
                    (i->second.req->data().get())[j] = (line.data.get())[get_offset(addr)+j];
                }
                i->second.status = REQ_DONE;
                if (++m_transferred_this_cycle == m_cfgs.block_per_cycle) {
                    break;
                }
            }
        }
    }
}

void cache::process() {
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
        if (i->second.status == REQ_BUSY) {
            assert(i->second.remaining_process_time > 0);
            --(i->second.remaining_process_time);
            if (i->second.remaining_process_time == 0) {
                shared_ptr<memoryRequest> req = i->second.req;
                if (hit(req->addr()) && m_transferred_this_cycle < m_cfgs.block_per_cycle) {
                    cache_line_t line = cache_line(req->addr());
                    // copy data 
                    i->second.status = REQ_DONE;
                } else {
                    // make new request
                    //i->second.home_req_id = m_home->request(new_req);
                    i->second.status = REQ_WAIT;
                }
            }
        }
    }
}


