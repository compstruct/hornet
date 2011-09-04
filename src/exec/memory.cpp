// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memory.hpp"

/*****************/
/* memoryRequest */
/*****************/

memoryRequest::memoryRequest(maddr_t maddr, uint32_t word_count) :
    m_status(REQ_NEW), m_is_read(true), m_maddr(maddr), m_word_count(word_count),
    m_data(shared_array<uint32_t>()), 
    m_per_mem_instr_runtime_info(shared_ptr<shared_ptr<void> >())
{}

memoryRequest::memoryRequest(maddr_t maddr, uint32_t word_count, shared_array<uint32_t> wdata) :
    m_status(REQ_NEW), m_is_read(false), m_maddr(maddr), m_word_count(word_count),m_data(wdata), 
    m_per_mem_instr_runtime_info(shared_ptr<shared_ptr<void> >())
{}

memoryRequest::~memoryRequest() {}

/*****************/
/* memory        */
/*****************/

memory::memory(uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r) :
    m_id(id), system_time(t), stats(st), log(l), ran(r), m_dramctrl(NULL), m_dramctrl_location(id) 
{ }

memory::~memory() { 
    delete m_dramctrl;
}

void memory::add_local_dram_controller(shared_ptr<dram> connected_dram,
                                        uint32_t dram_controller_latency, uint32_t offchip_oneway_latency, uint32_t dram_latency,
                                        uint32_t msg_header_size_in_words, uint32_t max_requests_in_flight,
                                        uint32_t bandwidth_in_words_per_cycle, bool use_lock) {
    m_dramctrl_location = m_id;
    if (m_dramctrl != NULL) {
        delete m_dramctrl;
    } 
    m_dramctrl = new dramController(m_id, system_time, stats, log, ran,
                                           connected_dram, dram_controller_latency, offchip_oneway_latency, dram_latency,
                                           msg_header_size_in_words, max_requests_in_flight, bandwidth_in_words_per_cycle,
                                           use_lock);
}

void memory::set_remote_dram_controller(uint32_t location) {
    if (m_dramctrl != NULL) {
        delete m_dramctrl;
    }
    m_dramctrl_location = location;
}

