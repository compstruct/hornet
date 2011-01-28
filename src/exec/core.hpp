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
#include "remoteMemory.hpp"
#include "coreMessageQueue.hpp"

using namespace std;
using namespace boost;

#define MAX_PAYLOAD 256

/* core class provides common constructor process and connect process */

class core : public pe {
public:
    typedef struct {
        uint32_t msg_queue_size;
        uint32_t max_active_remote_mem_requests;
        uint32_t flits_per_mem_msg_header;
        uint32_t bytes_per_flit;
        uint32_t memory_server_process_time;
        bool     support_library;
    } core_cfg_t;

    core(const pe_id &id, const uint64_t &system_time,
         shared_ptr<id_factory<packet_id> > packet_id_factory,
         shared_ptr<tile_statistics> stats, logger &log,
         shared_ptr<random_gen> ran, core_cfg_t cfgs) throw(err);
    virtual ~core() throw();

    /* Common core methods */
    virtual void connect(shared_ptr<bridge> net_bridge) throw(err);
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);

    /* Fast forwarding */
    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_drained() const throw() = 0;

    /* Is not used */
    virtual void add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err);
    virtual bool work_queued() throw(err);
    virtual bool is_ready_to_offer() throw(err);
    virtual void set_stop_darsim() throw(err);

    void add_remote_memory(shared_ptr<remoteMemory> mem);
    void add_away_cache(shared_ptr<memory> cache);
    void add_to_memory_hierarchy(int level, shared_ptr<memory> mem);

protected:
    /* execute */
    virtual void exec_core() = 0;

    /* migration message queues */
    shared_ptr<coreMessageQueue> core_receive_queue(int channel);
    shared_ptr<coreMessageQueue> core_send_queue(int channel);

    inline shared_ptr<memory> nearest_memory() { return m_memory_hierarchy[m_min_memory_level]; }
    inline shared_ptr<remoteMemory> remote_memory() { return m_remote_memory; }
    inline shared_ptr<memory> away_cache() { return m_away_cache; }

protected:
    /* Global time */
    const uint64_t &system_time;

    /* Aux */
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;

    /* message queues */
    map<msg_type_t, shared_ptr<coreMessageQueue> > m_out_msg_queues;
    map<msg_type_t, shared_ptr<coreMessageQueue> > m_in_msg_queues;


private:
    void release_xmit_buffer();
    virtual void exec_mem_server(); /* memory server */

    typedef struct {
        packet_id id;
        flow_id flow;
        uint32_t len;
        uint32_t xmit;
        uint64_t payload[MAX_PAYLOAD];
    } core_incoming_packet_t;
    typedef map<uint32_t, core_incoming_packet_t> incoming_packets_t;

    typedef enum {
        REQ_INIT,
        REQ_BUSY,
        REQ_PROCESSED,
        REQ_WAIT,
        REQ_DONE
    } req_status_t;

    typedef struct {
        req_status_t status;
        msg_type_t msg_type;
        int sender;
        uint32_t target_level;
        shared_ptr<memoryRequest> req;
        uint32_t remaining_process_time;
    } server_in_req_entry_t; 
    
    typedef struct {
        int sender;
        mreq_id_t in_req_id;
        shared_ptr<memory> mem_to_serve;
    } server_local_req_entry_t;

    /* for now table per sender... in hardware only needs unique in_req_ids for each sender */
    map<int, shared_ptr<map<mreq_id_t, server_in_req_entry_t> > > m_server_in_req_table;
    map<int, shared_ptr<map<mreq_id_t, server_local_req_entry_t> > > m_server_local_req_table;

    core_cfg_t m_cfgs;

    /* Network */
    shared_ptr<bridge> m_net;
    vector<uint32_t> m_queue_ids;
    incoming_packets_t m_incoming_packets;
    shared_ptr<id_factory<packet_id> > m_packet_id_factory;
    map<uint32_t, uint64_t*> m_xmit_buffer;
    
    /* Memories */
    shared_ptr<remoteMemory> m_remote_memory;
    shared_ptr<memory> m_away_cache;
    map<int, shared_ptr<memory> > m_memory_hierarchy;
    int m_max_memory_level;
    int m_min_memory_level;

    /* memory server */
    uint32_t m_num_active_remote_mem_requests;

    /* Aux */
    uint32_t m_current_receive_channel;

};

/* TODO (Phase 4) : Design core stats */

#endif
