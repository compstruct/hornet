// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "catStrip.hpp"

catStrip::catStrip(uint32_t num_cores, uint32_t chunk_size) : m_num_cores(num_cores), m_chunk_size(chunk_size) {}
catStrip::~catStrip() {}

uint32_t catStrip::getCoreID(shared_ptr<memoryRequest> req) { return (req->addr()/m_chunk_size)%m_num_cores; }


