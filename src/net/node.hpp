// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __NODE_HPP__
#define __NODE_HPP__

#include <map>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "logger.hpp"
#include "virtual_queue.hpp"
#include "router.hpp"
#include "ingress.hpp"
#include "egress.hpp"
#include "crossbar.hpp"

using namespace std;
using namespace boost;

class node {
public:
    explicit node(node_id id, uint32_t flits_per_queue,
                  shared_ptr<router> new_router,
                  shared_ptr<channel_alloc> new_vc_alloc,
                  shared_ptr<statistics> stats, logger &log) throw();
    const node_id &get_id() const throw();
    void add_ingress(node_id src, shared_ptr<ingress> ingress) throw(err);
    void add_egress(node_id dst, shared_ptr<egress> egress) throw(err);
    void add_queue_id(virtual_queue_id id) throw(err);
    shared_ptr<ingress> get_ingress_from(node_id src) throw(err);
    shared_ptr<egress> get_egress_to(node_id dst) throw(err);
    shared_ptr<router> get_router() throw();
    shared_ptr<channel_alloc> get_channel_alloc() throw();
    shared_ptr<pressure_tracker> get_pressures() throw();
    void connect_from(const string &port_name,
                      shared_ptr<node> src, const string &src_port_name,
                      const set<virtual_queue_id> &vq_ids, unsigned link_bw,
                      unsigned bw_to_xbar) throw(err);
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
private:
    const node_id id;
    unsigned flits_per_queue;
    typedef map<node_id, shared_ptr<ingress> > ingresses_t;
    typedef map<node_id, shared_ptr<egress> > egresses_t;
    shared_ptr<router> rt;
    shared_ptr<channel_alloc> vc_alloc;
    shared_ptr<pressure_tracker> pressures;
    ingresses_t ingresses;
    egresses_t egresses;
    crossbar xbar;
    set<virtual_queue_id> queue_ids;
    logger &log;
};

inline const node_id &node::get_id() const throw() { return id; }

#endif // __NODE_HPP__

