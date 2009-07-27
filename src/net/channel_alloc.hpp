// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CHANNEL_ALLOC_HPP__
#define __CHANNEL_ALLOC_HPP__

#include <utility>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "virtual_queue_id.hpp"
#include "ingress.hpp"
#include "egress.hpp"
#include "logger.hpp"

using namespace std;

class channel_alloc {
public:
    virtual ~channel_alloc() throw();
    virtual void claim(const virtual_queue_node_id &q) throw(err);
    virtual void release(const virtual_queue_node_id &q) throw(err);
    virtual bool is_claimed(const virtual_queue_node_id &q) throw(err);
    virtual void allocate() throw(err) = 0;
    virtual void add_ingress(shared_ptr<ingress> ingress) throw(err);
    virtual void add_egress(shared_ptr<egress> egress) throw(err) = 0;
    const node_id &get_id() const throw();
protected:
    channel_alloc(node_id src, bool one_queue_per_flow, bool one_flow_per_queue,
                  logger &log) throw();
protected:
    const node_id id;
    bool one_queue_per_flow;
    bool one_flow_per_queue;
    typedef vector<shared_ptr<ingress> > ingresses_t;
    ingresses_t ingresses;
private:
    set<virtual_queue_node_id> in_use;
protected:
    logger &log;
};

inline const node_id &channel_alloc::get_id() const throw() { return id; }

#endif // __CHANNEL_ALLOC_HPP__

