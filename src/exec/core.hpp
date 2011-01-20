// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CORE_HPP__
#define __CORE_HPP__

#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include "id_factory.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "pe.hpp"
#include "random.hpp"
#include "bridge.hpp"
#include "message.hpp"
#include "memory.hpp"

using namespace std;
using namespace boost;

/* core class provides common constructor process and connect process */

class core : public pe {
public:
    core(const pe_id &id, const uint64_t &system_time,
         shared_ptr<id_factory<packet_id> > packet_id_factory,
         shared_ptr<tile_statistics> stats, logger &log,
         shared_ptr<random_gen> ran) throw(err);
    virtual ~core() throw();

    /* Common core methods */
    virtual void connect(shared_ptr<bridge> net_bridge) throw(err);
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);

    /* Fast forwarding */
    virtual uint64_t next_pkt_time() throw(err) = 0;
    virtual bool is_drained() const throw() = 0;

    /* Is not used */
    virtual void add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err);
    virtual bool work_queued() throw(err);
    virtual bool is_ready_to_offer() throw(err);
    virtual void set_stop_darsim() throw(err);

    /* TODO (Phase 5) : Design configurator methods */

protected:
    /* execute */
    virtual void exec_core() = 0;

    /* Subclasses are responsible to 'register' any first-level memories using this method */
    void add_first_level_memory(shared_ptr<memory> mem);

private:
    void release_xmit_buffer();
    virtual void exec_mem_server(); /* memory server */

protected:
    typedef struct {
        packet_id id;
        flow_id flow;
        uint32_t len;
        uint32_t xmit;
        uint64_t *payload;
    } core_incoming_packet_t;
    typedef map<uint32_t, core_incoming_packet_t> incoming_packets_t;

    /* Global time */
    const uint64_t &system_time;

    /* Network */
    shared_ptr<bridge> m_net;
    vector<uint32_t> m_queue_ids;
    incoming_packets_t m_incoming_packets;
    shared_ptr<id_factory<packet_id> > m_packet_id_factory;
    map<uint32_t, uint64_t*> m_xmit_buffer;
    
    /* message queues */
    vector<message*> m_in_mig_msg_queue;
    vector<message*> m_in_mem_msg_queue;
    vector<message*> m_out_mig_msg_queue;
    vector<message*> m_out_mem_msg_queue;

    /* Memories */
    vector<shared_ptr<memory> > m_first_memories;

    /* Aux */
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;

};

/* TODO (Phase 4) : Design core stats */

#endif
