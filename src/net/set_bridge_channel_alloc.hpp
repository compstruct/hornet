// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SET_BRIDGE_CHANNEL_ALLOC_HPP__
#define __SET_BRIDGE_CHANNEL_ALLOC_HPP__

#include <map>
#include "bridge_channel_alloc.hpp"
#include "par_random.hpp"

using namespace std;

class set_bridge_channel_alloc : public bridge_channel_alloc {
public:
    set_bridge_channel_alloc(node_id src, bool one_q_per_f, bool one_f_per_q,
                             logger &log, shared_ptr<BoostRand> ran) throw();
    virtual ~set_bridge_channel_alloc() throw();
    virtual virtual_queue_id request(flow_id flow) throw(err);
    void add_queue(shared_ptr<virtual_queue> q) throw(err);
    void add_route(const flow_id &f,
                   const vector<tuple<virtual_queue_id,double> > &qs)
        throw(err);
private:
    typedef map<virtual_queue_id, shared_ptr<virtual_queue> > queues_t;
    queues_t queues;
    typedef vector<tuple<shared_ptr<virtual_queue>, double> > route_queues_t;
    typedef map<flow_id, route_queues_t> routes_t;
    routes_t routes;
    shared_ptr<BoostRand> ran;
};

#endif // __SET_BRIDGE_CHANNEL_ALLOC_HPP__
