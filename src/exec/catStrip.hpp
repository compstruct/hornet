// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CAT_STRIP_HPP__
#define __CAT_STRIP_HPP__

#include "cat.hpp"

class catStrip : public cat {
public:
    catStrip(uint32_t num_cores, uint32_t chunk_size);
    virtual ~catStrip();

    virtual uint32_t getCoreID(shared_ptr<memoryRequest> req);

private:
    uint32_t m_num_cores;
    uint64_t m_chunk_size;
};
#endif
