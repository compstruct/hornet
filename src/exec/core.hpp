// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CORE_HPP__
#define __CORE_HPP__

#include <vector>
#include <map>
#include <memory>
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

/* common_core class provides common constructor process and connect process */

#define MAX_PAYLOAD 256

class common_core : public pe {
public:
    common_core(const pe_id &id, const uint64_t &system_time,
                std::shared_ptr<id_factory<packet_id> > packet_id_factory,
                std::shared_ptr<tile_statistics> stats, logger &log,
                std::shared_ptr<random_gen> ran, 
                std::shared_ptr<memory> mem,
                uint32_t number_of_core_msg_types, uint32_t msg_queue_size, uint32_t m_bytes_per_flit);
    virtual ~common_core();

    /* Common core methods */
    virtual void connect(std::shared_ptr<bridge> net_bridge);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* Fast forwarding */
    virtual uint64_t next_pkt_time();
    virtual bool is_drained() const = 0;

    /* not used */
    virtual void add_packet(uint64_t time, const flow_id &flow, uint32_t len);
    virtual bool work_queued();
    virtual bool is_ready_to_offer();
    virtual void set_stop_darsim();

protected:
    /* implement core logic */
    virtual void execute() = 0;
    /* check and update finished memory requests */
    virtual void update_from_memory_requests() = 0;
    void tick_positive_edge_memory();
    void tick_negative_edge_memory();

    /* use these queues to use the network */
    std::shared_ptr<messageQueue> receive_queue(uint32_t type);
    std::shared_ptr<messageQueue> send_queue(uint32_t type);

    /* Global time */
    const uint64_t &system_time;

    /* Aux */
    std::shared_ptr<tile_statistics> stats;
    logger &log;
    std::shared_ptr<random_gen> ran;

    uint32_t m_number_of_msg_types;
    uint32_t m_first_core_msg_type;
    uint32_t m_msg_queue_size;
    uint32_t m_bytes_per_flit;

    /* memory */
    std::shared_ptr<memory> m_memory;

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

    map<uint32_t/*msg type*/, std::shared_ptr<messageQueue> > m_out_msg_queues;
    map<uint32_t/*msg type*/, std::shared_ptr<messageQueue> > m_in_msg_queues;

    /* Network */
    std::shared_ptr<bridge> m_net;
    vector<uint32_t> m_queue_ids;
    incoming_packets_t m_incoming_packets;
    std::shared_ptr<id_factory<packet_id> > m_packet_id_factory;
    map<uint32_t, uint64_t*> m_xmit_buffer;

    uint32_t m_receive_channel_round_robin_pointer;
    uint32_t m_send_queue_round_robin_pointer;

};

#endif

