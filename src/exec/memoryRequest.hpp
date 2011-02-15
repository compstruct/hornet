// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_REQUEST_HPP__
#define __MEMORY_REQUEST_HPP__

#include "stdio.h"
#include "cstdint.hpp"

using namespace std;

typedef uint64_t mreq_id_t;
#define MAX_INVALID_MREQ_ID 0

typedef enum {
    MEM_READ = 0,
    MEM_WRITE
} mreq_type_t;

typedef uint64_t maddr_t;

class memoryRequest {
public:
    memoryRequest(int tid, maddr_t addr, uint32_t byte_count, uint32_t* wdata = NULL);
    virtual ~memoryRequest();

    inline mreq_type_t rw() { return m_rw; }
    inline maddr_t addr() { return m_addr; }
    inline uint32_t byte_count() { return m_byte_count; }
    inline int tid() { return m_tid; }
    inline bool is_ra() { return m_is_ra; }
    inline void set_ra() { m_is_ra = true; }

    inline uint32_t* data() { return m_data; }
    inline uint64_t timestamp() { return m_timestamp; }
    inline void set_timestamp(uint64_t timestamp) { m_timestamp = timestamp; }

    /* LIBRARY COMPETITION */
    /* you may add additional information to send to home, but be careful about hardware expenses and network bandwidth */
    /* example */
    /* inline uint32_t first_info() { return m_first_info; } */
    /* inline void set_first_info(uint32_t info) { m_first_info = info; } */
        

protected:
    mreq_type_t   m_rw;
    int           m_tid;
    maddr_t       m_addr;
    uint32_t*     m_data;
    uint32_t      m_byte_count;
    uint64_t      m_timestamp;
    bool          m_is_ra;

    /* LIBRARY COMPETITION */
    /* add instance variables for additional information */
    /* example */
    /* uint32_t m_first_info; */
};

#endif
