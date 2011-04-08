// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memory.hpp"

/*****************/
/* memoryRequest */
/*****************/

memoryRequest::memoryRequest(maddr_t maddr, uint32_t word_count) :
    m_status(REQ_WAIT), m_is_read(true), m_maddr(maddr), m_word_count(word_count),
    m_data(shared_array<uint32_t>()) 
{}

memoryRequest::memoryRequest(maddr_t maddr, uint32_t word_count, shared_array<uint32_t> wdata) :
    m_status(REQ_WAIT), m_is_read(false), m_maddr(maddr), m_word_count(word_count),m_data(wdata) 
{}

memoryRequest::~memoryRequest() {}

/*****************/
/* memory        */
/*****************/

memory::memory(uint32_t id, const uint64_t &t, shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r) :
    m_id(id), system_time(t), stats(st), log(l), ran(r) { }

    memory::~memory() { }

