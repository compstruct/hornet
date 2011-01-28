// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memory.hpp"

memory::memory(const uint32_t id, const uint32_t level,
               const uint64_t &t, shared_ptr<tile_statistics> st,
               logger &l,
               shared_ptr<random_gen> r)
    : m_id(id), m_level(level),
      system_time(t),
      stats(st),
      log(l),
      ran(r), 
      m_max_mreq_id(MAX_INVALID_MREQ_ID) { 
}

memory::~memory() {}

mreq_id_t memory::take_new_mreq_id() {
    if (m_mreq_id_pool.empty()) {
        return ++m_max_mreq_id;
    } else {
        mreq_id_t ret = m_mreq_id_pool.front();
        m_mreq_id_pool.erase(m_mreq_id_pool.begin());
        return ret;
    }
}

void memory::return_mreq_id(mreq_id_t old_id) {
    if (false) {
        /* temporarily disable reusing mreq_id to process memory requests in order */
        /* TODO : inside memory classes, replace map with two vectors */
        m_mreq_id_pool.push_back(old_id);
    }
}

mreq_id_t memory::request(shared_ptr<memoryRequest> req) {
    return request(req, m_id, m_level);
}
