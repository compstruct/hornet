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
        uint32_t num_nodes;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
        uint32_t max_local_cache_requests_in_flight;
        uint32_t max_remote_cache_requests_in_flight;
        uint32_t max_local_directory_requests_in_flight;
        uint32_t max_remote_directory_requests_in_flight;
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

    virtual bool available();
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
        WAITING_FOR_REPLIES,
        WAITING_FOR_DRAM
    } directoryCoherenceStatus_t;

    typedef struct {
        bool hold;
        directoryCoherenceStatus_t status;
        set<uint32_t> directory;
    } directoryCoherenceInfo;

    typedef enum {
        SH_REQ = 0,
        EX_REQ,
        INV_REQ,
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
        coherenceMsgType_t type;
        maddr_t maddr;
        shared_array<uint32_t> data;
    } coherenceMsg;

private:
    typedef enum {
        _TO_L1_WAIT_L1 = 0,
        _TO_L1_WAIT_CAT,
        _TO_L1_SEND_REQ,
        _TO_L1_SEND_REP,
        _TO_L1_WAIT_REP,
        _TO_L1_FEED_L1
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
        uint64_t requested_time;
    } toL1Entry;

    typedef enum {
        _TO_L2_READ_L2 = 0,
        _TO_L2_WAIT_INFO,
        _TO_L2_SEND_REQ,
        _TO_L2_WAIT_REP,
        _TO_L2_INV_L2,
        _TO_L2_SEND_DRAM,
        _TO_L2_WAIT_DRAM,
        _TO_L2_FEED_L2,
        _TO_L2_SEND_REP
    } toL2Status;

    typedef struct {
        toL2Status status;
        shared_ptr<coherenceMsg> cache_req;
        shared_ptr<cacheRequest> l2_req;
        shared_ptr<coherenceMsg> cache_rep;
        shared_ptr<dramRequest> dram_req;
    } toL2Entry;

    typedef vector<shared_ptr<toL1Entry> > toL1Queue;
    typedef map<maddr_t, toL1Queue> toL1Table;

    typedef vector<shared_ptr<toL2Entry> > toL2Queue;
    typedef map<maddr_t, toL2Queue> toL2Table;

    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { 
        maddr.address -= (maddr.address)%m_cfg.words_per_cache_line; return maddr; 
    }

    void schedule_requests();

    void to_l1_table_update();
    void to_l2_table_update();
    void accept_incoming_messages();

    privateSharedMSICfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    shared_ptr<cat> m_cat;

    shared_ptr<privateSharedMSIStatsPerTile> m_stats;

    toL1Table m_to_l1_table;
    toL2Table m_to_l2_table;

    /* to-entry table request scheduler */
    vector<shared_ptr<toL1Entry> > m_to_l1_req_schedule_q;
    vector<shared_ptr<toL2Entry> > m_to_l2_req_schedule_q;

    /* to-memory request scheduler */
    vector<shared_ptr<catRequest> > m_cat_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l1_read_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l1_write_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l2_read_req_schedule_q;
    vector<shared_ptr<cacheRequest> > m_l2_write_req_schedule_q;
    vector<shared_ptr<dramRequest> > m_dram_req_schedule_q;

    vector<shared_ptr<coherenceMsg> > m_local_cache_reps_this_cycle;

    uint32_t m_to_l1_table_local_quota;
    uint32_t m_to_l1_table_remote_quota;
    uint32_t m_to_l2_table_local_quota;
    uint32_t m_to_l2_table_remote_quota;
};

#endif
