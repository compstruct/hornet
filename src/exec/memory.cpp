// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memory.hpp"

memory::memory(const uint32_t numeric_id, const uint32_t num_tiles,
               const uint64_t &t,
               logger &l,
               shared_ptr<random_gen> r)
    : system_time(t),
      log(l),
      ran(r) { 
    uint32_t num_id_bits = 0;
    for (uint32_t t = num_tiles-1; t; ++num_id_bits, t >>= 1);
    uint32_t req_id_bits = sizeof(mem_req_id_t) * CHAR_BIT;
    uint32_t id_offset = req_id_bits - num_id_bits;
    mem_req_id_t mask = ~((1ULL << id_offset) - 1ULL);
    mem_req_id_t templ =
        static_cast<mem_req_id_t>(numeric_id) << id_offset;
    m_req_id_factory =
        shared_ptr<id_factory<mem_req_id_t> >(new id_factory<mem_req_id_t>(templ, mask));
}
