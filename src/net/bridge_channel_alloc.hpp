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
    virtual ~bridge_channel_alloc() throw();
    // returns a vqid q s.t. !q.is_valid() if it's being claimed
    virtual virtual_queue_id request(flow_id flow) throw(err) = 0;
    virtual void claim(const virtual_queue_node_id &q) throw(err);
    virtual void release(const virtual_queue_node_id &q) throw(err);
    virtual bool is_claimed(const virtual_queue_node_id &q) throw(err);
    const node_id &get_id() const throw();
protected:
    bridge_channel_alloc(node_id src, logger &log) throw();
protected:
    const node_id id;
private:
    set<virtual_queue_node_id> in_use;
protected:
    logger &log;
};

inline const node_id &bridge_channel_alloc::get_id() const throw() {
    return id;
}

#endif // __BRIDGE_CHANNEL_ALLOC_HPP__
