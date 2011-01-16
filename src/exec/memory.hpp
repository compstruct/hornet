// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#include <boost/shared_ptr.hpp>
#include "bridge.hpp"
#include "id_factory.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "random.hpp"

using namespace std;
using namespace boost;

typedef uint64_t mem_req_id_t;

typedef enum {
    MEM_READ = 0,
    MEM_WRITE
} mem_req_type_t;

typedef uint64_t mem_addr_t;

class memory {
public:
    memory(const uint32_t numeric_id, const uint32_t num_tiles,
           const uint64_t &system_time,
           logger &log,
           shared_ptr<random_gen> ran);
    virtual ~memory();

    /* Ticking - core is responsible to tick memory */
    virtual void tick_positive_edge() = 0;
    virtual void tick_negative_edge() = 0;

    /* Memory operations */
    virtual mem_req_id_t request(mem_req_type_t rw, mem_addr_t addr, void* data, uint32_t num_bytes, bool cacheable) = 0;
    virtual bool ready(mem_req_id_t id) = 0;
    virtual bool finish(mem_req_id_t id) = 0;

    /* Network queues */
    //virtual message* nextMessageToSend() = 0;

protected:
    /* Global time */
    const uint64_t &system_time;

    /* Aux */
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;

    /* Memory */
    shared_ptr<id_factory<mem_req_id_t> > m_req_id_factory;

    /* Network */
};

/* TODO (Phase 4) : Design memory stats */

#endif
