// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#include <utility>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "virtual_queue_id.hpp"
#include "logger.hpp"

using namespace std;

class router {
public:
    virtual ~router() throw();
    virtual node_id route(node_id src_node_id, flow_id flow) throw(err) = 0;
    const node_id &get_id() const throw();
protected:
    router(node_id id, logger &log) throw();
protected:
    const node_id id;
    logger &log;
};

inline const node_id &router::get_id() const throw() { return id; }

#endif // __ROUTER_HPP__

