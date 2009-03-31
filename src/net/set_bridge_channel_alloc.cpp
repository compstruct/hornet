// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "random.hpp"
#include "set_bridge_channel_alloc.hpp"

set_bridge_channel_alloc::set_bridge_channel_alloc(node_id n, logger &l) throw()
    : bridge_channel_alloc(n, l), queues(), routes() { }

set_bridge_channel_alloc::~set_bridge_channel_alloc() throw() { }

void set_bridge_channel_alloc::add_queue(shared_ptr<virtual_queue> q)
    throw(err) {
    virtual_queue_id qid = q->get_id().get<1>();
    if (queues.find(qid) != queues.end())
        throw err_duplicate_queue(get_id().get_numeric_id(),
                                  qid.get_numeric_id());
    queues[qid] = q;
}

void set_bridge_channel_alloc::add_route(const flow_id &f,
                                         const vector<virtual_queue_id> &qids)
    throw(err) {
    if (routes.find(f) != routes.end())
        throw err_duplicate_flow(get_id().get_numeric_id(), f.get_numeric_id());
    if (!qids.empty()) { // add only specified queues
        for (vector<virtual_queue_id>::const_iterator idi = qids.begin();
             idi != qids.end(); ++idi) {
            queues_t::iterator qi = queues.find(*idi);
            if (qi == queues.end())
                throw err_bad_next_hop_queue(get_id().get_numeric_id(),
                                             f.get_numeric_id(),
                                             get_id().get_numeric_id(),
                                             idi->get_numeric_id());
            routes[f].push_back(qi->second);
        }
    } else { // add all queues
        for(queues_t::iterator qi = queues.begin(); qi != queues.end(); ++qi) {
            routes[f].push_back(qi->second);
        }
    }
}

virtual_queue_id set_bridge_channel_alloc::request(flow_id f) throw(err) {
    if (routes.find(f) == routes.end())
        throw exc_bad_flow(get_id().get_numeric_id(), f.get_numeric_id());
    const vector<shared_ptr<virtual_queue> > &qs = routes[f];
    vector<shared_ptr<virtual_queue> > free_qs;
    for (vector<shared_ptr<virtual_queue> >::const_iterator qi = qs.begin();
         qi != qs.end(); ++qi) {
        if (!is_claimed((*qi)->get_id()) && (*qi)->ingress_new_flow()) {
            free_qs.push_back(*qi);
        }
    }
    if (!free_qs.empty()) {
        int n = free_qs.size() == 1 ? 0 : random_range(free_qs.size());
        return free_qs[n]->get_id().get<1>();
    } else {
        return virtual_queue_id(); // invalid ID
    }
}
