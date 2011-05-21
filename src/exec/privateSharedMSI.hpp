// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_MSI_HPP__
#define __PRIVATE_SHARED_MSI_HPP__

#include <set>
#include "memory.hpp"
#include "cache.hpp"
#include "cat.hpp"
#include "privateSharedMSIStats.hpp"

class privateSharedMSI : public memory {
public:
    typedef struct {
        bool     use_mesi;
        uint32_t num_nodes;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
        uint32_t address_size_in_bytes;
        uint32_t num_local_core_ports;
        uint32_t l1_work_table_size;
        uint32_t l2_work_table_size_shared;
        uint32_t l2_work_table_size_replies;
        uint32_t l2_work_table_size_evict;
        /* L1 */
        uint32_t lines_in_l1;
        uint32_t l1_associativity;
        uint32_t l1_hit_test_latency;
        uint32_t l1_num_read_ports;
        uint32_t l1_num_write_ports;
        replacementPolicy_t l1_replacement_policy;
        /* L2 */
        uint32_t lines_in_l2;
        uint32_t l2_associativity;
        uint32_t l2_hit_test_latency;
        uint32_t l2_num_read_ports;
        uint32_t l2_num_write_ports;
        replacementPolicy_t l2_replacement_policy;
    } privateSharedMSICfg_t;

    privateSharedMSI(uint32_t numeric_id, 
                     const uint64_t &system_time, 
                     shared_ptr<tile_statistics> stats, 
                     logger &log, 
                     shared_ptr<random_gen> ran, 
                     shared_ptr<cat> a_cat, 
                     privateSharedMSICfg_t cfg);
    virtual ~privateSharedMSI();

    virtual uint32_t number_of_mem_msg_types();

    virtual void request(shared_ptr<memoryRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* set stats */
    inline void set_per_tile_stats(shared_ptr<privateSharedMSIStatsPerTile> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != shared_ptr<privateSharedMSIStatsPerTile>()); }
    inline shared_ptr<privateSharedMSIStatsPerTile> stats() { return m_stats; }

    typedef enum {
        MSG_DRAM_REQ = 0,
        MSG_DRAM_REP,
        MSG_CACHE_REQ,
        MSG_CACHE_REP,
        MSG_DIRECTORY_REQ_REP,
        NUM_MSG_TYPES
    } privateSharedMSIMsgType_t;

    typedef enum {
        MODIFIED = 0,
        SHARED,
        INVALID, 
        PENDING 
    } cacheCoherenceStatus_t;
    
    typedef struct {
        cacheCoherenceStatus_t status;
        uint32_t directory_home;
    } cacheCoherenceInfo;

    typedef enum {
        READERS = 0,
        WRITER,
        WAITING_FOR_REPLIES
    } directoryCoherenceStatus_t;

    typedef struct {
        directoryCoherenceStatus_t status;
        set<uint32_t> directory;
    } directoryCoherenceInfo;

    typedef enum {
        SH_REQ = 0,
        EX_REQ,
        INV_REQ,
        EMPTY_REQ, /* intra-directory request */
        WB_REQ,
        FLUSH_REQ,
        SH_REP,
        EX_REP,
        INV_REP,
        WB_REP,
        FLUSH_REP
    } coherenceMsgType_t;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        coherenceMsgType_t type;
        maddr_t maddr;
        shared_array<uint32_t> data;
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
        _L1_WORK_READ_L1 = 0,
        _L1_WORK_SEND_CACHE_REP,
        _L1_WORK_READ_CAT,
        _L1_WORK_SEND_CACHE_REQ,
        _L1_WORK_WAIT_DIRECTORY_REP,
        _L1_WORK_FEED_L1_AND_FINISH,
        _L1_WORK_INVALIDATE_SHARED_LINE
    } toL1Status;

    typedef struct {
        toL1Status status;
        shared_ptr<memoryRequest> core_req;
        shared_ptr<cacheRequest> l1_req;
        shared_ptr<catRequest> cat_req;
        shared_ptr<coherenceMsg> dir_req;
        shared_ptr<coherenceMsg> dir_rep;
        shared_ptr<coherenceMsg> cache_req;
        shared_ptr<coherenceMsg> cache_rep;

        /* for stats */
        uint64_t requested_time;

        /* for performance */
        shared_ptr<message_t> net_msg_to_send;

    } toL1Entry;

    typedef enum {
        _L2_WORK_READ_L2 = 0,
        _L2_WORK_UPDATE_L2_AND_FINISH,
        _L2_WORK_EMPTY_LINE_TO_EVICT,
        _L2_WORK_REORDER_CACHE_REP,
        _L2_WORK_INVALIDATE_CACHES,
        _L2_WORK_UPDATE_L2_AND_SEND_REP,
        _L2_WORK_DRAM_WRITEBACK_AND_UPDATE,
        _L2_WORK_DRAM_WRITEBACK_AND_REQUEST,
        _L2_WORK_DRAM_WRITEBACK_AND_EVICT,
        _L2_WORK_SEND_DRAM_FEED_REQ,
        _L2_WORK_WAIT_DRAM_FEED,
        _L2_WORK_SEND_DIRECTORY_REP
    } toL2Status;

    typedef struct {
        toL2Status status;
        bool accept_cache_replies;
        shared_ptr<coherenceMsg> cache_req;
        shared_ptr<cacheRequest> l2_req;
        shared_ptr<coherenceMsg> cache_rep;
        vector<shared_ptr<coherenceMsg> > dir_reqs;;
        shared_ptr<coherenceMsg> dir_rep;
        shared_ptr<dramMsg> dram_req;
        shared_ptr<dramMsg> dram_rep;

        bool using_space_for_reply;
        bool using_space_for_evict;

        /* for stats */
        uint64_t invalidate_begin_time;
        uint32_t invalidate_num_targets;
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

    /* logics */

    void schedule_requests();

    void l1_work_table_update();

    void l2_work_table_update();
    void process_cache_rep(shared_ptr<cacheLine> line, shared_ptr<coherenceMsg> rep);

    void dram_work_table_update();

    void accept_incoming_messages();

    /* instance variables */

    privateSharedMSICfg_t m_cfg;
    shared_array<uint32_t> GLOB; // printf

    cache* m_l1;
    cache* m_l2;
    shared_ptr<cat> m_cat;

    shared_ptr<privateSharedMSIStatsPerTile> m_stats;

    toL1Table m_l1_work_table;
    toL2Table m_l2_work_table;
    toDRAMTable m_dram_work_table;

    /* scheduler queues */
    vector<shared_ptr<memoryRequest> > m_core_port_schedule_q; 
    vector<tuple<bool/* is a memoryRequest type */, shared_ptr<void> > > m_to_cache_req_schedule_q; 

    /* scheduler queues for requests in to l2 work table */
    vector<shared_ptr<coherenceMsg> > m_to_directory_req_schedule_q;
    vector<shared_ptr<coherenceMsg> > m_to_directory_rep_schedule_q;

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

    uint32_t m_l1_work_table_vacancy;
    uint32_t m_l2_work_table_vacancy_shared;
    uint32_t m_l2_work_table_vacancy_replies; /* need at least one dedicated space for accepting an L1 reply */
    uint32_t m_l2_work_table_vacancy_evict; /* need at least one dedicated space for invalidating L1 caches for eviction */
    uint32_t m_available_core_ports;
};

#endif

