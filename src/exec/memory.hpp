// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#include <boost/shared_ptr.hpp>
#include "bridge.hpp"
#include "logger.hpp"
#include "random.hpp"
#include "memoryRequest.hpp"

using namespace std;
using namespace boost;

class memory {
public:
    memory(const uint32_t numeric_id, 
           const uint64_t &system_time,
           logger &log,
           shared_ptr<random_gen> ran);
    virtual ~memory();

    /* Memory operations - users only use these functions */
    virtual mreq_id_t request(shared_ptr<memoryRequest> req) = 0;
    virtual bool ready(mreq_id_t id) = 0;
    virtual bool finish(mreq_id_t id) = 0;

    /* Ticking - base class (core) sends ticks first-level memory components */
    /* if a memory component has sub-level components, it is responsible to tick them */
    virtual void initiate() = 0;
    virtual void update() = 0;
    virtual void process() = 0;

protected:
    mreq_id_t take_new_mreq_id();
    void return_mreq_id(mreq_id_t old_id);

protected:
    /* numeric id */
    uint32_t m_id;

    /* Global time */
    const uint64_t &system_time;

    /* Aux */
    logger &log;
    shared_ptr<random_gen> ran;

    /* mreq_id */
    mreq_id_t m_max_mreq_id;
    vector<mreq_id_t> m_mreq_id_pool;
};

/* TODO (Phase 4) : Design memory stats */

#endif
