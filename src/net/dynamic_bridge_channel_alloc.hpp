// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __DYNAMIC_BRIDGE_CHANNEL_ALLOC_HPP__
#define __DYNAMIC_BRIDGE_CHANNEL_ALLOC_HPP__

#include <map>
#include "virtual_queue.hpp"
#include "bridge_channel_alloc.hpp"

using namespace std;

class dynamic_bridge_channel_alloc : public bridge_channel_alloc {
public:
    dynamic_bridge_channel_alloc(node_id src, logger &log) throw();
    virtual ~dynamic_bridge_channel_alloc() throw();
    virtual virtual_queue_id request(flow_id flow) throw(err);
    void add_queue(shared_ptr<virtual_queue> q) throw(err);
private:
    typedef vector<shared_ptr<virtual_queue> > queues_t;
    queues_t queues;
};

#endif // __DYNAMIC_BRIDGE_CHANNEL_ALLOC_HPP__
