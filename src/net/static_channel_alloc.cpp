// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include "random.hpp"
#include "static_channel_alloc.hpp"

static_channel_alloc::static_channel_alloc(node_id n, logger &l) throw()
    : channel_alloc(n, l), routes() { }

static_channel_alloc::~static_channel_alloc() throw() { }

void static_channel_alloc::allocate() throw(err) {
    typedef vector<shared_ptr<virtual_queue> > qs_t;
    qs_t qs;
    for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end();
         ++ii) {
        const ingress::queues_t &iqs = (*ii)->get_queues();
        for (ingress::queues_t::const_iterator qi = iqs.begin();
             qi != iqs.end(); ++qi) {
            if (qi->second->egress_new_flow()
                && !qi->second->front_vq_id().is_valid()) {
                qs.push_back(qi->second);
            }
        }
    }
    LOG(log,12) << "[alloc " << get_id() << "] allocating grants for "
                << dec << qs.size() << " ingress queue"
                << (qs.size() == 1 ? "" : "s") << endl;
    random_shuffle(qs.begin(), qs.end(), random_range);
    for (qs_t::iterator qi = qs.begin(); qi != qs.end(); ++qi) {
        assert((*qi)->egress_new_flow());
        assert(!(*qi)->front_vq_id().is_valid());
        flow_id f = (*qi)->get_egress_flow_id();
        node_id n = (*qi)->front_node_id();
        assert(routes.find(n) != routes.end());
        map<flow_id, virtual_queue_id> &f2vq = routes[n];
        if (f2vq.find(f) == f2vq.end())
            throw exc_bad_flow(get_id().get_numeric_id(), f.get_numeric_id());
        virtual_queue_id &q = f2vq[f];
        if (!is_claimed(make_tuple(n,q))) (*qi)->set_front_vq_id(q);
    }
}

void static_channel_alloc::add_route(node_id dst, flow_id f,
                                     virtual_queue_id q) throw(err) {
    map<flow_id, virtual_queue_id> &f2vq = routes[dst];
    if (f2vq.find(f) != f2vq.end())
        throw err_duplicate_flow(get_id().get_numeric_id(),
                                 f.get_numeric_id());
    LOG(log,4) << "alloc " << get_id() << " routing flow " << f
        << " on node " << dst << " to queue " << q << endl;
    f2vq[f] = q;
}

void static_channel_alloc::add_egress(shared_ptr<egress> egress) throw(err) { }
