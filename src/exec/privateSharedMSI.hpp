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
        emType_t em_type;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
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

    privateSharedMSI(uint32_t numeric_id, const uint64_t &system_time, shared_ptr<tile_statistics> stats, logger &log, 
            shared_ptr<random_gen> ran, shared_ptr<cat> a_cat, privateSharedMSICfg_t cfg);
    virtual ~privateSharedMSI();

    virtual uint32_t number_of_mem_msg_types();

    virtual void request(shared_ptr<memoryRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* set stats */
    inline void set_per_tile_stats(shared_ptr<privateSharedMSIStatsPerTile> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != shared_ptr<privateSharedMSIStatsPerTile>()); }
    inline shared_ptr<privateSharedMSIStatsPerTile> stats() { return m_stats; }

private:
    typedef enum {
        MSG_DRAM_REQ = 0,
        MSG_DRAM_REP,
        MSG_CC_CACHE_REQ,
        MSG_CC_DIRECTORY_REQ,
        MSG_CC_REP,
        NUM_MSG_TYPES
    } privateSharedMSIMsgType_t;

    typedef enum {
        MODIFIED = 0,
        SHARED,
        INVALID,
        PENDING
    } cacheCoherenceInfo_t;

    typedef enum {
        READERS = 0,
        WRITER,
        WAITING_FOR_READERS,
        WAITING_FOR_WRITER
    } directoryCoherenceStatus_t;

    typedef struct {
        directoryCoherenceStatus_t status;
        set<uint32_t> directory;
    } directoryCoherenceInfo_t;

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
    } coherenceMsgFlag_t;

    typedef struct {
        uint32_t sender;
        coherenceMsgFlag_t flag;
        maddr_t first_maddr;
        shared_array<uint32_t> data;
    } coherenceMsg_t;

    typedef struct {
        shared_ptr<coherenceMsg_t> msg;
        shared_ptr<cacheRequest> cache_request;
    } coherenceMsgTableEntry_t;

    typedef vector<coherenceMsgTableEntry_t> coherenceMsgTable;

    typedef struct {
        uint32_t sender;
        shared_ptr<dramRequest> dram_request;
    } dramMsgTableEntry_t;

    typedef vector<dramMsgTableEntry_t> dramMsgTable;

    typedef struct {
        shared_ptr<memoryRequest> mem_request;
        shared_ptr<cacheRequest> cache_request;
        shared_ptr<catRequest> cat_request;
    } memReqTableEntry_t;

    typedef vector<memReqTableEntry_t> memReqTable;

    privateSharedMSICfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    shared_ptr<cat> m_cat;

    memReqTable m_mem_req_table;
    coherenceMsgTable m_cache_coherence_req_table;
    coherenceMsgTable m_directory_coherence_req_table;
    coherenceMsgTable m_coherence_reply_table;

    shared_ptr<privateSharedMSIStatsPerTile> m_stats;
};

#endif
