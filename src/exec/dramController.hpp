// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __DRAM_CONTROLLER_HPP__
#define __DRAM_CONTROLLER_HPP__

#include "memory.hpp"
#include "logger.hpp"
#include "random.hpp"
#include <boost/shared_ptr.hpp>

using namespace boost;

class dramController : public memory {
public:
    dramController(const uint32_t numeric_id, const uint64_t &system_time,
          logger &log, shared_ptr<random_gen> ran,
          bool use_lock);
    virtual ~dramController();

    virtual mreq_id_t request(shared_ptr<memoryRequest> req);
    virtual bool ready(mreq_id_t id);
    virtual bool finish(mreq_id_t id);

    virtual void initiate();
    virtual void update();
    virtual void process();

private:
    bool m_protected;
};

#endif
