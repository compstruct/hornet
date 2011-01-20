// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memoryRequest.hpp"
#include <iostream>

memoryRequest::memoryRequest(mreq_type_t rw, maddr_t addr, shared_ptr<uint32_t> data, uint32_t byte_count)
    : m_rw(rw), m_addr(addr), m_data(data), m_byte_count(byte_count) { }

memoryRequest::~memoryRequest() { }
