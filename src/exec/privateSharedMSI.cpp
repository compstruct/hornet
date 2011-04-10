// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSI.hpp"

#define COHERENCE_INFO_SPACE UINT32_MAX

#define DBG_PRINT

privateSharedMSI::privateSharedMSI(uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, 
                 shared_ptr<random_gen> r, shared_ptr<cat> a_cat, privateSharedMSICfg_t cfg) :
    memory(id, t, st, l, r), m_cfg(cfg), m_l1(NULL), m_l2(NULL), m_cat(a_cat), m_stats(shared_ptr<privateSharedMSIStatsPerTile>())
{
    assert(m_cfg.bytes_per_flit > 0);
    assert(m_cfg.words_per_cache_line > 0);
    /* create caches */
    assert(m_cfg.lines_in_l1 > 0);
    assert(m_cfg.lines_in_l2 > 0);

    m_l1 = new cache(id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l1, cfg.l1_associativity, cfg.l1_replacement_policy, 
                     cfg.l1_hit_test_latency, cfg.l1_num_read_ports, cfg.l1_num_write_ports);
    m_l2 = new cache(id, t, st, l, r, 
                     cfg.words_per_cache_line, cfg.lines_in_l2, cfg.l2_associativity, cfg.l2_replacement_policy, 
                     cfg.l2_hit_test_latency, cfg.l2_num_read_ports, cfg.l2_num_write_ports);
}

privateSharedMSI::~privateSharedMSI() {
    delete m_l1;
    delete m_l2;
}

uint32_t privateSharedMSI::number_of_mem_msg_types() { return NUM_MSG_TYPES; }

void privateSharedMSI::request(shared_ptr<memoryRequest> req) {

    /* set status to wait */
    set_req_status(req, REQ_WAIT);

    /* make a cache request */
    maddr_t maddr =  req->maddr();
    cacheReqType_t cache_req_type = (req->is_read())? CACHE_REQ_READ : CACHE_REQ_WRITE;
    shared_ptr<cacheRequest> cache_req = shared_ptr<cacheRequest>(new cacheRequest(maddr, cache_req_type));
    m_l1->request(cache_req);

    /* make a CAT request */
    shared_ptr<catRequest> cat_req = shared_ptr<catRequest>(new catRequest(maddr, m_id));
    m_cat->request(cat_req);

    /* make an entry */
    memReqTableEntry_t new_entry = {req, cache_req, cat_req, system_time};
    m_mem_req_table.push_back(new_entry);
    
}

void privateSharedMSI::tick_positive_edge() {

    /* dealing with incoming messages */

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

    /* update results from l1 */
    memReqTable::iterator it_mem_req;
    for (it_mem_req = m_mem_req_table.begin(); it_mem_req != m_mem_req_table.end(); ++it_mem_req) {
        if (it_mem_req->cache_request->status() == CACHE_WAIT) {
            continue;
        } else if (it_mem_req->cache_request->status() == CACHE_MISS) {
            if (!it_mem_req->mem_request->is_read()) {
                // TODO : write on the cache line 
            }
            set_req_status(it_mem_req->mem_request, REQ_DONE);
        } else if (it_mem_req->cache_request->status() == CACHE_HIT) {
            if (!it_mem_req->mem_request->is_read()) {
                // TODO : write on the cache line 
            }
            set_req_status(it_mem_req->mem_request, REQ_DONE);
        }
    }

}

