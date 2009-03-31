// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATIC_BRIDGE_CHANNEL_ALLOC_HPP__
#define __STATIC_BRIDGE_CHANNEL_ALLOC_HPP__

#include <map>
#include "bridge_channel_alloc.hpp"

using namespace std;

class static_bridge_channel_alloc : public bridge_channel_alloc {
public:
    static_bridge_channel_alloc(node_id src, logger &log) throw();
    virtual ~static_bridge_channel_alloc() throw();
    virtual virtual_queue_id request(flow_id flow) throw(err);
    void add_route(flow_id, virtual_queue_id vq) throw(err);
private:
    map<flow_id, virtual_queue_id> routes;
};

#endif // __STATIC_BRIDGE_CHANNEL_ALLOC_HPP__
