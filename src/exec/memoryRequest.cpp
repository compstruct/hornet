// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memoryRequest.hpp"

memoryRequest::memoryRequest(mem_req_type_t rw, mem_addr_t addr, shared_ptr<void> data, uint32_t byte_count)
    : m_rw(rw), m_addr(addr), m_data(data), m_byte_count(byte_count) { }

