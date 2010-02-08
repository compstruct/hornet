// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SET_CHANNEL_ALLOC_HPP__
#define __SET_CHANNEL_ALLOC_HPP__

#include <map>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include "channel_alloc.hpp"
#include "egress.hpp"
#include "par_random.hpp"

class set_channel_alloc : public channel_alloc {
public:
    set_channel_alloc(node_id src, bool one_queue_per_flow,
                      bool one_flow_per_queue, logger &log, shared_ptr<BoostRand> ran) throw();
    virtual ~set_channel_alloc() throw();
    virtual void allocate() throw(err);
    virtual void add_egress(shared_ptr<egress> egress) throw(err);
    void add_route(const node_id &src, const flow_id &f, const node_id &dst, 
                   const flow_id &nf,
                   const vector<tuple<virtual_queue_id,double> > &qs)
        throw(err);
private:
    typedef map<node_id, shared_ptr<egress> > egresses_t;
    egresses_t egresses;
    typedef tuple<node_id, flow_id, node_id, flow_id> route_query_t;
    typedef vector<tuple<shared_ptr<virtual_queue>, double> > route_queues_t;
    typedef map<route_query_t, route_queues_t> routes_t;
    routes_t routes;
    shared_ptr<BoostRand> ran;
};

#endif // __SET_CHANNEL_ALLOC_HPP__
