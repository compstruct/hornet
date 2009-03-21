// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __DYNAMIC_CHANNEL_ALLOC_HPP__
#define __DYNAMIC_CHANNEL_ALLOC_HPP__

#include <set>
#include <map>
#include "channel_alloc.hpp"
#include "egress.hpp"

class dynamic_channel_alloc : public channel_alloc {
public:
    dynamic_channel_alloc(node_id src, logger &log) throw();
    virtual ~dynamic_channel_alloc() throw();
    virtual virtual_queue_id request(node_id node, flow_id flow) throw(err);
    virtual void release(virtual_queue_id q) throw(err);
    void add_egress(node_id dst, shared_ptr<egress> egress) throw(err);
private:
    typedef deque<shared_ptr<virtual_queue> > vq_deque_t;
    typedef map<node_id, vq_deque_t> egress_queues_t;
    egress_queues_t egresses;
    set<virtual_queue_id> in_use;
};

#endif // __DYNAMIC_CHANNEL_ALLOC_HPP__

