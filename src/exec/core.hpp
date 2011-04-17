// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CORE_HPP__
#define __CORE_HPP__

#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include "id_factory.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "pe.hpp"
#include "random.hpp"
#include "bridge.hpp"
#include "messages.hpp"
#include "memory.hpp"

using namespace std;
using namespace boost;

/* core class provides common constructor process and connect process */

#define MAX_PAYLOAD 256

class core : public pe {
public:
    core(const pe_id &id, const uint64_t &system_time,
         shared_ptr<id_factory<packet_id> > packet_id_factory,
         shared_ptr<tile_statistics> stats, logger &log,
         shared_ptr<random_gen> ran, 
         shared_ptr<memory> mem,
         uint32_t number_of_core_msg_types, uint32_t msg_queue_size, uint32_t m_bytes_per_flit) throw(err);
    virtual ~core() throw();

    /* Common core methods */
    virtual void connect(shared_ptr<bridge> net_bridge) throw(err);
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);

    /* Fast forwarding */
    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_drained() const throw() = 0;

    /* not used */
    virtual void add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err);
    virtual bool work_queued() throw(err);
    virtual bool is_ready_to_offer() throw(err);
    virtual void set_stop_darsim() throw(err);

protected:
    /* implement core logic */
    virtual void execute() = 0;
    /* check and update finished memory requests */
    virtual void update_from_memory_requests() = 0;

    /* use these queues to use the network */
    shared_ptr<messageQueue> receive_queue(uint32_t type);
    shared_ptr<messageQueue> send_queue(uint32_t type);

    /* Global time */
    const uint64_t &system_time;

    /* Aux */
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;

    uint32_t m_number_of_msg_types;
    uint32_t m_first_core_msg_type;
    uint32_t m_msg_queue_size;
    uint32_t m_bytes_per_flit;

    /* memory */
    shared_ptr<memory> m_memory;

private:
    typedef struct {
        packet_id id;
        flow_id flow;
        uint32_t len;
        uint32_t xmit;
        uint64_t payload[MAX_PAYLOAD];
    } core_incoming_packet_t;
    typedef map<uint32_t, core_incoming_packet_t> incoming_packets_t;

    void release_xmit_buffer();

    map<uint32_t/*msg type*/, shared_ptr<messageQueue> > m_out_msg_queues;
    map<uint32_t/*msg type*/, shared_ptr<messageQueue> > m_in_msg_queues;

    /* Network */
    shared_ptr<bridge> m_net;
    vector<uint32_t> m_queue_ids;
    incoming_packets_t m_incoming_packets;
    shared_ptr<id_factory<packet_id> > m_packet_id_factory;
    map<uint32_t, uint64_t*> m_xmit_buffer;

    uint32_t m_receive_channel_round_robin_pointer;
    uint32_t m_send_queue_round_robin_pointer;

};

#endif

