// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATIC_CHANNEL_ALLOC_HPP__
#define __STATIC_CHANNEL_ALLOC_HPP__

#include <set>
#include <map>
#include "channel_alloc.hpp"

class static_channel_alloc : public channel_alloc {
public:
    static_channel_alloc(node_id src, logger &log) throw();
    virtual ~static_channel_alloc() throw();
    virtual virtual_queue_id request(node_id node, flow_id flow) throw(err);
    virtual void release(virtual_queue_id q) throw(err);
    void add_route(node_id dst, flow_id, virtual_queue_id vq) throw(err);
private:
    map<node_id, map<flow_id, virtual_queue_id> > routes;
    set<virtual_queue_id> in_use;
};

#endif // __STATIC_CHANNEL_ALLOC_HPP__
