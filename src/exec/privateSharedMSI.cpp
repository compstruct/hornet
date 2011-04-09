// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSI.hpp"

#define COHERENCE_INFO_SPACE UINT32_MAX

privateSharedMSI::privateSharedMSI(uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, 
                 shared_ptr<random_gen> r, shared_ptr<cat> a_cat, privateSharedMSICfg_t cfg) :
    memory(id, t, st, l, r), m_cfg(cfg), m_l1(NULL), m_l2(NULL), m_cat(a_cat)
{
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

}

void privateSharedMSI::tick_positive_edge() {

    // TODO

    m_l1->tick_positive_edge();
    m_l2->tick_positive_edge();
    if(m_dram_controller) {
        m_dram_controller->tick_positive_edge();
    }
}

void privateSharedMSI::tick_negative_edge() {

    m_l1->tick_positive_edge();
    m_l2->tick_positive_edge();
    if(m_dram_controller) {
        m_dram_controller->tick_positive_edge();
    }

    // TODO

}

