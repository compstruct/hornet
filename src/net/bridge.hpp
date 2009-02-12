// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __BRIDGE_HPP__
#define __BRIDGE_HPP__

#include <vector>
#include <map>
#include <utility>
#include <iostream>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include "error.hpp"
#include "logger.hpp"
#include "clockable.hpp"
#include "virtual_queue.hpp"

using namespace std;
using namespace boost;

class dma_channel_id {
public:
    dma_channel_id(uint32_t new_id) throw();
    uint32_t get_numeric_id() const throw();
    friend ostream &operator<<(ostream &, const dma_channel_id &);
private:
    explicit dma_channel_id() throw();
    uint32_t id;
};

inline dma_channel_id::dma_channel_id(uint32_t new_id) throw() : id(new_id) {}

inline uint32_t dma_channel_id::get_numeric_id() const throw() { return id; }

ostream &operator<<(ostream &, const dma_channel_id &);

class dma_channel : public clockable {
public:
    dma_channel(node_id node_id, dma_channel_id dma_id, uint32_t bandwidth,
                shared_ptr<logger> log = shared_ptr<logger>(new logger()))
        throw();
    bool busy() const throw();
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
    void send(flow_id flow, shared_ptr<virtual_queue> vq,
              void *src, uint32_t num_flits) throw(err);
    void receive(void *dst, shared_ptr<virtual_queue> vq, uint32_t num_flits)
        throw(err);
    const dma_channel_id &get_id() const throw();
private:
    const pair<node_id, dma_channel_id> id;
    const uint32_t bandwidth;
    bool egress;
    bool started;
    flow_id flow;
    uint32_t remaining_flits;
    void *mem;
    shared_ptr<virtual_queue> vq;
    shared_ptr<logger> log;
private:
    dma_channel(); // not defined
    dma_channel(const dma_channel &); // not defined
};

inline bool dma_channel::busy() const throw() { return remaining_flits > 0; }

inline const dma_channel_id &dma_channel::get_id() const throw() {
    return id.second;
}

class node;

class bridge : public clockable {
public:
    explicit bridge(node_id new_id, uint32_t num_dmas, uint32_t dma_bw,
                    shared_ptr<logger> log = shared_ptr<logger>(new logger()))
        throw(err);
    uint32_t get_transmission_done(uint32_t transmission) throw();
    uint32_t get_waiting_queues() throw();
    uint32_t get_queue_flow_id(uint32_t queue) throw(err);
    uint32_t get_queue_length(uint32_t queue) throw(err);
    // both() send and receive() return 0 on failure (no transfer started)
    // or transfer ID which can be used to check for completion later
    // which can be used later to check status
    uint32_t send(uint32_t flow_id, void *src, uint32_t num_flits) throw(err);
    uint32_t receive(void *dst, uint32_t queue, uint32_t num_flits) throw(err);

    void connect(shared_ptr<node> q) throw(err);
    void add_queue(shared_ptr<virtual_queue> q) throw(err);
    void add_route(flow_id flow, virtual_queue_id node_queue_id) throw(err);
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
private:
    const node_id id;
    typedef map<virtual_queue_id, shared_ptr<virtual_queue> > queues_t;
    typedef map<flow_id, shared_ptr<virtual_queue> > routes_t;
    typedef vector<shared_ptr<dma_channel> > dmas_t;

    dmas_t dmas;
    queues_t vqs;
    routes_t routes; // vqid is next hop's queue id
    shared_ptr<node> target;

    shared_ptr<logger> log;
private:
    bridge(); // not defined
    bridge(const bridge &); // not defined
};


#endif // __BRIDGE_HPP__

