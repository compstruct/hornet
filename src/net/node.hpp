// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __NODE_HPP__
#define __NODE_HPP__

#include <map>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "logger.hpp"
#include "clockable.hpp"
#include "virtual_queue.hpp"

using namespace std;
using namespace boost;

class node;
class bridge;
class arbiter;

class connection : public clockable {
public:
    explicit connection(node_id parent_id, shared_ptr<node> target,
                        unsigned max_bandwidth,
                        shared_ptr<logger> log =
                            shared_ptr<logger>(new logger())) throw();
    void add_queue(shared_ptr<virtual_queue> q) throw(err);
    void add_route(flow_id flow, virtual_queue_id neighbor_queue_id) throw(err);
    void set_bandwidth(unsigned new_bandwidth) throw();
    unsigned get_bandwidth() const throw();
    double get_pressure() throw();
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
private:
    const node_id parent_id;
    typedef map<virtual_queue_id, shared_ptr<virtual_queue> > queues_t;
    typedef map<flow_id, shared_ptr<virtual_queue> > routes_t;
    unsigned bandwidth; // in flits/tick
    shared_ptr<node> target;
    queues_t vqs;
    routes_t routes; // vqid is next hop's queue id
    shared_ptr<logger> log;
};

inline void connection::set_bandwidth(unsigned new_bandwidth) throw() {
    bandwidth = new_bandwidth;
}

inline unsigned connection::get_bandwidth() const throw() { return bandwidth; }

class node : public clockable {
public:
    explicit node(node_id id, uint32_t mem_size,
                  shared_ptr<logger> log =
                      shared_ptr<logger>(new logger())) throw();
    void connect(shared_ptr<node> target, unsigned bandwidth,
                 vector<virtual_queue_id> queues) throw(err);
    void connect(shared_ptr<bridge> target,
                 vector<virtual_queue_id> queues) throw(err);
    shared_ptr<connection> get_link_to(node_id neighbor) throw(err);
    void add_incoming_link(const node_id &from) throw(err);
    void add_route(flow_id flow, node_id neighbor_id,
                   virtual_queue_id neighbor_queue_id) throw(err);
    const shared_ptr<virtual_queue> get_virtual_queue(virtual_queue_id id)
        throw(err);
    const node_id &get_id() const throw();
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
private:
    const node_id id;
    typedef map<virtual_queue_id, shared_ptr<virtual_queue> > queues_t;
    typedef map<node_id, shared_ptr<connection> > links_t;
    unsigned flits_per_queue;
    queues_t vqs;
    links_t links;
    unsigned num_incoming_links;
    unsigned stale_num_slots; // resynchronized at negative clock edge
    shared_ptr<logger> log;
};

inline const node_id &node::get_id() const throw() { return id; }

#endif // __NODE_HPP__

