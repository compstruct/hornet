// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include "statistics.hpp"
#include "logger.hpp"
#include "random.hpp"

using namespace std;
using namespace boost;

typedef struct {
    uint32_t mem_space_id;
    uint64_t address;
} maddr_t;

typedef enum {
    REQ_WAIT = 0,
    REQ_DONE,
    REQ_MIGRATE
} memReqStatus_t;

class memoryRequest {
public:
    /* for reads */
    memoryRequest(maddr_t maddr, uint32_t word_count);
    /* for writes */
    memoryRequest(maddr_t maddr, uint32_t word_count, shared_array<uint32_t> wdata);
    ~memoryRequest();

    inline memReqStatus_t status() { return m_status; }
    inline maddr_t maddr() { return m_maddr; }
    inline shared_array<uint32_t> data() { return m_data; }
    inline bool is_read() { return m_is_read; }
    inline uint32_t home() { return m_home; }
    inline uint32_t word_count() { return m_word_count; }

    friend class memory;

private:

    memReqStatus_t m_status;
    bool m_is_read;
    maddr_t m_maddr;
    uint32_t m_word_count;
    shared_array<uint32_t> m_data;
    uint32_t m_home;

};

class memory {
public:
    memory(uint32_t numeric_id, 
           const uint64_t &system_time,
           shared_ptr<tile_statistics> stats,
           logger &log,
           shared_ptr<random_gen> ran);
    virtual ~memory();

    virtual void request(shared_ptr<memoryRequest> req) = 0;
    virtual void tick_positive_edge() = 0;
    virtual void tick_negative_edge() = 0;

    virtual uint32_t number_of_mem_msg_types() = 0;
protected:
    uint32_t m_id;
    const uint64_t &system_time;
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;

};

#endif
