// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __BRIDGE_CHANNEL_ALLOC_HPP__
#define __BRIDGE_CHANNEL_ALLOC_HPP__

#include <utility>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "virtual_queue_id.hpp"
#include "ingress.hpp"
#include "logger.hpp"

using namespace std;

class bridge_channel_alloc {
public:
    virtual ~bridge_channel_alloc();
    // returns a vqid q s.t. !q.is_valid() if it's being claimed
    virtual virtual_queue_id request(flow_id flow) = 0;
    virtual void claim(const virtual_queue_node_id &q);
    virtual void release(const virtual_queue_node_id &q);
    virtual bool is_claimed(const virtual_queue_node_id &q);
    const node_id &get_id() const;
protected:
    bridge_channel_alloc(node_id src, bool one_queue_per_flow,
                         bool one_flow_per_queue, logger &log);
protected:
    const node_id id;
    bool one_queue_per_flow;
    bool one_flow_per_queue;
private:
    set<virtual_queue_node_id> in_use;
protected:
    logger &log;
};

inline const node_id &bridge_channel_alloc::get_id() const {
    return id;
}

#endif // __BRIDGE_CHANNEL_ALLOC_HPP__
