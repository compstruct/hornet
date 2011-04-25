// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_LCC_HPP__
#define __PRIVATE_SHARED_LCC_HPP__

#include <set>
#include "memory.hpp"
#include "cache.hpp"
#include "cat.hpp"
#include "privateSharedLCCStats.hpp"

class privateSharedLCC : public memory {
public:
    typedef enum {
        _REPLACE_LRU = 0,
        _REPLACE_RANDOM = 1,
        _REPLACE_NEAREST_EXPIRATION_AND_LRU = 2,
        _REPLACE_NEAREST_EXPIRATION_AND_RANDOM = 3
    } _replacementPolicy_t;

    typedef enum {
        TIMESTAMP_FIXED = 0,
        TIMESTAMP_IDEAL,
        TIMESTAMP_ZERO_DELAY
    } timestampLogic_t;

    typedef struct {
        timestampLogic_t logic;
        bool save_timestamp_in_dram; /* saving timestamp in DRAM is NEVER tested and probably (99%) woudln't work */
        bool use_separate_vc_for_writes;
        uint32_t default_timestamp_delta;
        uint32_t num_nodes;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
        uint32_t address_size_in_bytes;
        uint32_t num_local_core_ports;
        uint32_t l2_work_table_size_shared;
        uint32_t l2_work_table_size_readonly;
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
    } privateSharedLCCCfg_t;

    privateSharedLCC(uint32_t numeric_id, 
                     const uint64_t &system_time, 
                     shared_ptr<tile_statistics> stats, 
                     logger &log, 
                     shared_ptr<random_gen> ran, 
                     shared_ptr<cat> a_cat, 
                     privateSharedLCCCfg_t cfg);
    virtual ~privateSharedLCC();

    virtual uint32_t number_of_mem_msg_types();

    virtual void request(shared_ptr<memoryRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* set stats */
    inline void set_per_tile_stats(shared_ptr<privateSharedLCCStatsPerTile> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != shared_ptr<privateSharedLCCStatsPerTile>()); }
    inline shared_ptr<privateSharedLCCStatsPerTile> stats() { return m_stats; }

    typedef enum {
        MSG_DRAM_REQ = 0,
        MSG_DRAM_REP,
        MSG_LCC_REQ_1,
        MSG_LCC_REQ_2,
        MSG_LCC_REP,
        NUM_MSG_TYPES
    } privateSharedLCCMsgType_t;

    typedef struct {
        uint64_t expiration_time;
        shared_ptr<uint64_t> synched_expiration_time; /* only used in idealized timestamp logic */

        /* only used in L2 with some timestamp logic */
        uint32_t timestamp_delta; 
        uint32_t timestamp_iter;
        uint32_t timestamp_iter_record;

        /* for stats */
        uint64_t first_read_time_since_last_expiration;
        uint64_t first_evict_block_time;
    } coherenceInfo;

    typedef enum {
        READ_REQ = 0,
        WRITE_REQ,
        READ_REP,
        WRITE_REP
    } coherenceMsgType_t;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        coherenceMsgType_t type;
        uint32_t word_count; 
        maddr_t maddr;
        shared_array<uint32_t> data;

        uint64_t timestamp;
        shared_ptr<uint64_t> synched_timestamp;

        /* this messy flag conveniently lets the issuer of this message know whether it has to send it again */
        bool did_win_last_arbitration;

        /* debug purpose - erase later */
        uint64_t waited;
    } coherenceMsg;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        shared_ptr<dramRequest> req;
        bool did_win_last_arbitration;
    } dramMsg;

private:
    typedef enum {
        _L1_WORK_WAIT_L1 = 0,
        _L1_WORK_WAIT_CAT,
        _L1_WORK_UPDATE_L1,
        _L1_WORK_SEND_LCC_REQ,
        _L1_WORK_WAIT_LCC_REP,
    } toL1Status;

    typedef struct {
        toL1Status status;
        shared_ptr<memoryRequest> core_req;
        shared_ptr<catRequest> cat_req;
        shared_ptr<cacheRequest> l1_req;
        shared_ptr<coherenceMsg> lcc_req;
        shared_ptr<coherenceMsg> lcc_rep;

        /* for stats */
        uint64_t requested_time;

        /* for performance */
        shared_ptr<message_t> net_msg_to_send;

    } toL1Entry;

    typedef enum {
        _L2_WORK_WAIT_L2 = 0,
        _L2_WORK_SEND_LCC_REP_THEN_FINISH,
        _L2_WORK_SEND_LCC_REP_THEN_WAIT_FOR_WRITE,
        _L2_WORK_UPDATE_L2,
        _L2_WORK_SEND_DRAM_WRITEBACK_THEN_FEED,
        _L2_WORK_SEND_DRAM_WRITEBACK_THEN_RETRY,
        _L2_WORK_SEND_DRAM_REQ,
        _L2_WORK_WAIT_DRAM_REP,
        _L2_WORK_WAIT_TIMESTAMP,
        _L2_WORK_WAIT_TIMESTAMP_AFTER_DRAM_FEED, /* better clean and long than messy and short */
        _L2_WORK_SEND_LCC_REP_THEN_WAIT_FOR_WRITE_AFTER_DRAM_FEED
    } toL2Status;

    typedef struct {
        toL2Status status;
        bool accept_read_requests;
        bool write_requests_waiting;
        shared_ptr<coherenceMsg> lcc_read_req;
        shared_ptr<coherenceMsg> lcc_write_req;
        shared_ptr<cacheRequest> l2_req;
        shared_ptr<coherenceMsg> lcc_rep;
        shared_ptr<dramMsg> dram_req;
        shared_ptr<dramMsg> dram_rep;

        bool using_space_for_reads;

        /* for stats */
        uint32_t write_blocked_time;
        bool did_miss_on_first;

        /* for performance */
        shared_ptr<message_t> net_msg_to_send;
    } toL2Entry;

    typedef struct {
        shared_ptr<dramMsg> dram_req;
        shared_ptr<dramMsg> dram_rep;
        /* for performance */
        shared_ptr<message_t> net_msg_to_send;
    } toDRAMEntry;

    typedef map<maddr_t, shared_ptr<toL1Entry> > toL1Table;
    typedef map<maddr_t, shared_ptr<toL2Entry> > toL2Table;
    typedef map<maddr_t, shared_ptr<toDRAMEntry> > toDRAMTable;

    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { 
        maddr.address -= (maddr.address)%(m_cfg.words_per_cache_line*4); return maddr; 
    }
    inline uint32_t get_flit_count(uint32_t bytes) { return (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit; }
    uint64_t get_expiration_time(shared_ptr<cacheLine> line);

    /* logics */

    void schedule_requests();

    void l1_work_table_update();

    void l2_work_table_update();

    void dram_work_table_update();

    void accept_incoming_messages();

    /* instance variables */

    privateSharedLCCCfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    shared_ptr<cat> m_cat;

    shared_ptr<privateSharedLCCStatsPerTile> m_stats;

    toL1Table m_l1_work_table;
    toL2Table m_l2_work_table;
    toDRAMTable m_dram_work_table;

    /* scheduler queues */
    vector<shared_ptr<memoryRequest> > m_core_port_schedule_q; 

    /* scheduler queues for requests in to l2 work table */
    vector<shared_ptr<coherenceMsg> > m_to_l2_read_req_schedule_q;
    vector<shared_ptr<coherenceMsg> > m_to_l2_write_req_schedule_q;

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

    uint32_t m_l2_work_table_vacancy_shared;
    uint32_t m_l2_work_table_vacancy_readonly;
    uint32_t m_available_core_ports;
};

#endif
