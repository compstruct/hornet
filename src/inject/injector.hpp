// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __INJECTOR_HPP__
#define __INJECTOR_HPP__

#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "cstdint.hpp"
#include "id_factory.hpp"
#include "flow_id.hpp"
#include "flit.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "pe.hpp"
#include "random.hpp"

using namespace std;
using namespace boost;

class waiting_packet {
public:
    packet_id id;
    flow_id flow;
    uint32_t len;
    bool count_in_stats;
};

class incoming_packet {
public:
    packet_id id;
    flow_id flow;
    uint32_t len;
    uint32_t xmit;
};

class injector : public pe {
public:
    injector(const pe_id &id, const uint64_t &system_time,
             shared_ptr<id_factory<packet_id> > packet_id_factory,
             shared_ptr<tile_statistics> stats, logger &log,
             shared_ptr<random_gen> ran) throw(err);
    virtual ~injector() throw();
    virtual void connect(shared_ptr<bridge> net_bridge) throw(err);
    virtual void add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err);
    virtual bool work_queued() throw(err);
    void add_event(const uint64_t &time, const flow_id &flow,
                   uint32_t len, const uint64_t &period) throw(err);
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
    virtual void set_stop_darsim() throw(err);
    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_ready_to_offer() throw(err);
    virtual bool is_drained() const throw();
private:
    typedef uint64_t tick_t;
    typedef uint32_t len_t;
    typedef uint64_t period_t;
    typedef vector<tuple<tick_t, flow_id, len_t, period_t> > events_t;
    typedef map<flow_id, tuple<tick_t, len_t, period_t> > flows_t;
    typedef map<flow_id, queue<waiting_packet> > waiting_packets_t;
    typedef map<uint32_t, incoming_packet> incoming_packets_t;

    const tick_t &system_time;
    shared_ptr<bridge> net;
    events_t events;
    events_t::iterator next_event;
    waiting_packets_t waiting_packets;
    incoming_packets_t incoming_packets;
    flows_t flows;
    vector<flow_id> flow_ids;
    vector<uint32_t> queue_ids;
    shared_ptr<id_factory<packet_id> > packet_id_factory;
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;
};

#endif // __INJECTOR_HPP__
