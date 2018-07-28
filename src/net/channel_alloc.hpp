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
    virtual ~channel_alloc();
    virtual void claim(const virtual_queue_node_id &q);
    virtual void release(const virtual_queue_node_id &q);
    virtual bool is_claimed(const virtual_queue_node_id &q);
    virtual void allocate() = 0;
    virtual void add_ingress(std::shared_ptr<ingress> ingress);
    virtual void add_egress(std::shared_ptr<egress> egress) = 0;
    const node_id &get_id() const;
protected:
    channel_alloc(node_id src, bool one_queue_per_flow, bool one_flow_per_queue,
                  logger &log);
protected:
    const node_id id;
    bool one_queue_per_flow;
    bool one_flow_per_queue;
    typedef vector<std::shared_ptr<ingress> > ingresses_t;
    ingresses_t ingresses;
private:
    set<virtual_queue_node_id> in_use;
protected:
    logger &log;
};

inline const node_id &channel_alloc::get_id() const { return id; }

#endif // __CHANNEL_ALLOC_HPP__

