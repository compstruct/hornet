// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_EMRA_HPP__
#define __PRIVATE_SHARED_EMRA_HPP__

#include <set>
#include "memory.hpp"
#include "cache.hpp"
#include "cat.hpp"
#include "privateSharedEMRAStats.hpp"

class privateSharedEMRA : public memory {
public:
    typedef enum {
        _REPLACE_LRU = 0,
        _REPLACE_RANDOM = 1,
    } _replacementPolicy_t;

    typedef enum {
        MIGRATION_NEVER = 0,
        MIGRATION_ALWAYS
    } emraLogic_t;

    typedef struct {
        emraLogic_t logic;
        uint32_t num_nodes;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
        uint32_t address_size_in_bytes;
        uint32_t num_local_core_ports;
        uint32_t work_table_size;
        /* L1 */
        uint32_t lines_in_l1;
        uint32_t l1_associativity;
        uint32_t l1_hit_test_latency;
        uint32_t l1_num_read_ports;
        uint32_t l1_num_write_ports;
        _replacementPolicy_t l1_replacement_policy;
        /* L2 */
        uint32_t lines_in_l2;
        uint32_t l2_associativity;
        uint32_t l2_hit_test_latency;
        uint32_t l2_num_read_ports;
        uint32_t l2_num_write_ports;
        _replacementPolicy_t l2_replacement_policy;
    } privateSharedEMRACfg_t;

    privateSharedEMRA(uint32_t numeric_id, 
                     const uint64_t &system_time, 
                     shared_ptr<tile_statistics> stats, 
                     logger &log, 
                     shared_ptr<random_gen> ran, 
                     shared_ptr<cat> a_cat, 
                     privateSharedEMRACfg_t cfg);
    virtual ~privateSharedEMRA();

    virtual uint32_t number_of_mem_msg_types();

    virtual void request(shared_ptr<memoryRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* set stats */
    inline void set_per_tile_stats(shared_ptr<privateSharedEMRAStatsPerTile> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != shared_ptr<privateSharedEMRAStatsPerTile>()); }
    inline shared_ptr<privateSharedEMRAStatsPerTile> stats() { return m_stats; }

    typedef enum {
        MSG_DRAM_REQ = 0,
        MSG_DRAM_REP,
        MSG_DATA_REQ,
        MSG_DATA_REP,
        NUM_MSG_TYPES
    } privateSharedEMRAMsgType_t;

    typedef struct {
        shared_ptr<bool> in_l1; /* evictions in L1 is snooped at L2 */
    } coherenceInfo;

    typedef enum {
        DATA_READ_REQ = 0,
        DATA_WRITE_REQ,
        DATA_REP,
    } coherenceMsgType_t;

    typedef struct {
        uint64_t mem_serialization;
        uint64_t cat_serialization;
        uint64_t cat_action;
        uint64_t l1_serialization;
        uint64_t l1_action;
        uint64_t l2_network;
        uint64_t l2_serialization;
        uint64_t l2_action;
        uint64_t dram_network_plus_serialization;
        uint64_t dram_offchip;

        uint64_t temp_cat_serialization;
        uint64_t temp_cat_action;
        uint64_t temp_l1_serialization;
        uint64_t temp_l1_action;
        uint64_t temp_l2_network;
        uint64_t temp_l2_serialization;
        uint64_t temp_l2_action;
        uint64_t temp_dram_network_plus_serialization;
        uint64_t temp_dram_offchip;

    } breakdownInfo;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        coherenceMsgType_t type;
        uint32_t word_count; 
        maddr_t maddr;
        shared_array<uint32_t> data;

        shared_ptr<bool> in_l1; /* evictions in L1 is snooped at L2 */

        /* this messy flag conveniently lets the issuer of this message know whether it has to send it again */
        bool did_win_last_arbitration;

        /* debug purpose - erase later */
        uint64_t waited;

        /* cost breakdown study */
        uint64_t milestone_time;
        shared_ptr<breakdownInfo> breakdown;

    } coherenceMsg;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        shared_ptr<dramRequest> req;
        bool did_win_last_arbitration;

        /* cost breakdown study */
        uint64_t milestone_time;
        shared_ptr<breakdownInfo> breakdown;

    } dramMsg;

private:

    typedef enum {
        _WORK_WAIT_CAT_AND_L1_FOR_LOCAL_READ = 0,
        _WORK_WAIT_CAT_AND_L1_FOR_LOCAL_WRITE,
        _WORK_WAIT_L2_FOR_LOCAL,
        _WORK_DRAM_WRITEBACK_AND_FEED,
        _WORK_SEND_DRAM_FEED_REQ,
        _WORK_WAIT_DRAM_FEED,
        _WORK_WAIT_L1_WRITE_AFTER_L2,
        _WORK_UPDATE_L1,
        _WORK_UPDATE_L1_AND_L2,
        _WORK_DRAM_WRITEBACK_AND_UPDATE_L1_AND_L2,

        _WORK_SEND_REMOTE_DATA_REQ,
        _WORK_WAIT_REMOTE_DATA_REP,
        _WORK_WAIT_L2_FOR_REMOTE_READ,
        _WORK_WAIT_L1_AND_L2_FOR_REMOTE_WRITE,
        _WORK_SEND_REMOTE_DATA_REP,
        _WORK_UPDATE_L2
    } entryStatus;

    typedef struct {
        entryStatus status;

        shared_ptr<memoryRequest> core_req;
        shared_ptr<coherenceMsg> data_req;

        shared_ptr<catRequest> cat_req;
        shared_ptr<cacheRequest> l1_req;
        shared_ptr<cacheRequest> l1_evict_req;
        shared_ptr<cacheRequest> l2_req;

        shared_ptr<coherenceMsg> data_rep;

        shared_ptr<dramMsg> dram_req;
        shared_ptr<dramMsg> dram_rep;

        /* for performance */
        shared_ptr<message_t> net_msg_to_send;

        /* cost breakdown study */
        uint64_t requested_time;
        uint64_t milestone_time;
        shared_ptr<breakdownInfo> breakdown;

    } tableEntry;

    typedef struct {
        shared_ptr<dramMsg> dram_req;
        shared_ptr<dramMsg> dram_rep;
        /* for performance */
        shared_ptr<message_t> net_msg_to_send;

        /* cost breakdown study */
        uint64_t milestone_time;
        shared_ptr<breakdownInfo> breakdown;
        
    } toDRAMEntry;

    typedef map<maddr_t, shared_ptr<tableEntry> > workTable;
    typedef map<maddr_t, shared_ptr<toDRAMEntry> > toDRAMTable;

    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { 
        maddr.address -= (maddr.address)%(m_cfg.words_per_cache_line*4); return maddr; 
    }

    inline uint32_t get_flit_count(uint32_t bytes) { return (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit; }
    uint64_t get_expiration_time(shared_ptr<cacheLine> line);

    /* logics */

    void schedule_requests();

    void work_table_update();

    void dram_work_table_update();

    void accept_incoming_messages();

    void apply_breakdown_info(shared_ptr<breakdownInfo> breakdown);

    /* instance variables */

    privateSharedEMRACfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    shared_ptr<cat> m_cat;

    shared_ptr<privateSharedEMRAStatsPerTile> m_stats;

    workTable m_work_table;
    toDRAMTable m_dram_work_table;

    /* scheduler queues */
    vector<shared_ptr<memoryRequest> > m_core_port_schedule_q; 
    vector<tuple<bool/* is a memoryRequest type */, shared_ptr<void> > > m_req_schedule_q;

    vector<shared_ptr<dramMsg> > m_to_dram_req_schedule_q;

    /* scheduler queues for sending messages to the network */
    map<uint32_t/* per channel*/, vector<shared_ptr<message_t> > > m_to_network_schedule_q;

    /* to-memory request scheduler */
    vector<shared_ptr<catRequest> > m_cat_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l1_read_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l1_write_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l2_read_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l2_write_req_schedule_q;
    vector<shared_ptr<dramRequest> > m_dram_req_schedule_q;

    uint32_t m_work_table_vacancy;
    uint32_t m_available_core_ports;

};

#endif
