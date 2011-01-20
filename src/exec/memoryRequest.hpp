// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_REQUEST_HPP__
#define __MEMORY_REQUEST_HPP__

#include <boost/shared_ptr.hpp>

using namespace std;
using namespace boost;

typedef uint64_t mreq_id_t;
#define MAX_INVALID_MREQ_ID 0

typedef enum {
    MEM_READ = 0,
    MEM_WRITE
} mreq_type_t;

typedef uint64_t maddr_t;

class memoryRequest {
public:
    memoryRequest(mreq_type_t rw, maddr_t addr, shared_ptr<uint32_t> data, uint32_t byte_count);
    virtual ~memoryRequest();

    inline maddr_t get_addr() { return m_addr; }

protected:
    mreq_type_t   m_rw;
    maddr_t       m_addr;
    shared_ptr<uint32_t> m_data;
    uint32_t         m_byte_count;
};

#endif
