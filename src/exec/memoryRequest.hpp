// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_REQUEST_HPP__
#define __MEMORY_REQUEST_HPP__

#include <boost/shared_ptr.hpp>

using namespace boost;

typedef uint64_t mem_req_id_t;
#define INVALID_MEM_REQ_ID UINT64_MAX

typedef enum {
    MEM_READ = 0,
    MEM_WRITE
} mem_req_type_t;

typedef uint64_t mem_addr_t;

class memory;

class memoryRequest {
public:
    memoryRequest(mem_req_type_t rw, mem_addr_t addr, shared_ptr<void> data, uint32_t byte_count);
    virtual ~memoryRequest();

    friend class memory;

protected:
    mem_req_type_t   m_rw;
    mem_addr_t       m_addr;
    shared_ptr<void> m_data;
    uint32_t         m_byte_count;
};

#endif
