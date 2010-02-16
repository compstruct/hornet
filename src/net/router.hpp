// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#include <vector>
#include <utility>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "virtual_queue_id.hpp"
#include "ingress.hpp"
#include "egress.hpp"
#include "logger.hpp"

using namespace std;
using namespace boost;

class router {
public:
    virtual ~router() throw();
    const node_id &get_id() const throw();
    virtual void add_ingress(shared_ptr<ingress> ingress) throw(err);
    virtual void add_egress(shared_ptr<egress> egress) throw(err) = 0;
    virtual void route() throw(err) = 0;
protected:
    router(node_id id, logger &log) throw();
protected:
    const node_id id;
    typedef vector<shared_ptr<ingress> > ingresses_t;
    ingresses_t ingresses;
    logger &log;
};

inline const node_id &router::get_id() const throw() { return id; }

#endif // __ROUTER_HPP__

