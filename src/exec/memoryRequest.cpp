// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memoryRequest.hpp"
#include <assert.h>

memoryRequest::memoryRequest(int tid, maddr_t addr, uint32_t byte_count, uint32_t* wdata)
    : m_tid(tid), m_addr(addr), m_byte_count(byte_count), m_timestamp(0), m_is_ra(false) { 

    /* address should be aligned in 32bit (4bytes) */
    assert(m_addr%4 == 0);
    /* byte_count should be aligned in 32bit (4bytes) */
    assert(m_byte_count%4 == 0);
    
    m_rw = (wdata)? MEM_WRITE : MEM_READ;
    m_data = new uint32_t[m_byte_count/4];
    if (wdata) {
        for (uint32_t i = 0; i < m_byte_count/4; ++i) {
            m_data[i] = wdata[i];
        }
    }
}

memoryRequest::~memoryRequest() { 
    delete[] m_data;
}
