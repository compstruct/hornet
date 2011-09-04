// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SHARED_SHARED_EMRA_HPP__
#define __SHARED_SHARED_EMRA_HPP__

#include <set>
#include "memory.hpp"
#include "cache.hpp"
#include "cat.hpp"
#include "sharedSharedEMRAStats.hpp"

class sharedSharedEMRA : public memory {
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
    } sharedSharedEMRACfg_t;

    sharedSharedEMRA(uint32_t numeric_id, 
                     const uint64_t &system_time, 
                     shared_ptr<tile_statistics> stats, 
                     logger &log, 
                     shared_ptr<random_gen> ran, 
                     shared_ptr<cat> a_cat, 
                     sharedSharedEMRACfg_t cfg);
    virtual ~sharedSharedEMRA();

    virtual uint32_t number_of_mem_msg_types();

    virtual void request(shared_ptr<memoryRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* set stats */
    inline void set_per_tile_stats(shared_ptr<sharedSharedEMRAStatsPerTile> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != shared_ptr<sharedSharedEMRAStatsPerTile>()); }
    inline shared_ptr<sharedSharedEMRAStatsPerTile> stats() { return m_stats; }

    typedef enum {
        MSG_DRAMCTRL_REQ = 0,
        MSG_DRAMCTRL_REP,
        MSG_RA_REQ,
        MSG_RA_REP,
        NUM_MSG_TYPES
    } sharedSharedEMRAMsgType_t;

    typedef enum {
        RA_READ_REQ = 0,
        RA_WRITE_REQ,
        RA_REP,
    } coherenceMsgType_t;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        coherenceMsgType_t type;
        uint32_t word_count; 
        maddr_t maddr;
        shared_array<uint32_t> data;

        /* status */
        bool sent;

        /* fot stats */
        shared_ptr<sharedSharedEMRAStatsPerMemInstr> per_mem_instr_stats;
        uint64_t birthtime;

    } coherenceMsg;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        shared_ptr<dramRequest> dram_req;
        maddr_t maddr;

        /* status */
        bool sent;

        /* for stats */
        uint64_t birthtime;
        shared_ptr<sharedSharedEMRAStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlMsg;

private:

    typedef enum {
        _CAT_AND_L1_FOR_LOCAL = 0,
        _L1_FOR_REMOTE,
        _L2,
        _FILL_L1,
        _FILL_L2,
        _SEND_DRAMCTRL_REQ,
        _WAIT_DRAMCTRL_REP,
        _WRITEBACK_L2,
        _WRITEBACK_DRAMCTRL_FOR_L1_FILL,
        _WRITEBACK_DRAMCTRL_FOR_L2_FILL,
        _SEND_RA_REQ,
        _SEND_RA_REP,
        _WAIT_RA_REP
    } entryStatus;

    typedef struct {
        entryStatus status;

        shared_ptr<memoryRequest> core_req;
        shared_ptr<catRequest> cat_req;
        shared_ptr<cacheRequest> l1_req;
        shared_ptr<cacheRequest> l2_req;
        shared_ptr<coherenceMsg> ra_req;
        shared_ptr<coherenceMsg> ra_rep;
        shared_ptr<dramctrlMsg> dramctrl_req;
        shared_ptr<dramctrlMsg> dramctrl_rep;

        /* for stats */
        shared_ptr<sharedSharedEMRAStatsPerMemInstr> per_mem_instr_stats;

    } tableEntry;

    typedef struct {
        shared_ptr<dramctrlMsg> dramctrl_req;
        shared_ptr<dramctrlMsg> dramctrl_rep;
        shared_ptr<message_t> net_msg_to_send;

        /* for stats */
        uint64_t operation_begin_time;
        shared_ptr<sharedSharedEMRAStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlTableEntry;

    typedef map<maddr_t, shared_ptr<tableEntry> > workTable;
    typedef map<maddr_t, shared_ptr<dramctrlTableEntry> > dramctrlTable;

    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { 
        maddr.address -= (maddr.address)%(m_cfg.words_per_cache_line*4); return maddr; 
    }

    inline uint32_t get_flit_count(uint32_t bytes) { return (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit; }

    /* logics */

    void schedule_requests();
    void update_work_table();
    void update_dramctrl_work_table();
    void accept_incoming_messages();

    /* instance variables */

    sharedSharedEMRACfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    shared_ptr<cat> m_cat;

    shared_ptr<sharedSharedEMRAStatsPerTile> m_stats;

    workTable m_work_table;
    dramctrlTable m_dramctrl_work_table;

    uint32_t m_work_table_vacancy;
    uint32_t m_available_core_ports;

    /* keep track of writeback requests to priortize it. no real hw, for simulation performance */
    map<maddr_t, shared_ptr<tableEntry> > m_l2_writeback_status;
    map<maddr_t, shared_ptr<tableEntry> > m_dramctrl_writeback_status;

    /* SCHEDULER QUEUES */
    /* no real hardware... volatile at clock boundaries */

    vector<shared_ptr<memoryRequest> > m_core_port_schedule_q; 
    vector<tuple<bool/* is remote */, shared_ptr<void> > > m_new_work_table_entry_schedule_q;

    vector<shared_ptr<tableEntry> > m_cat_req_schedule_q;
    vector<shared_ptr<tableEntry> > m_l1_read_req_schedule_q;
    vector<shared_ptr<tableEntry> > m_l1_write_req_schedule_q;
    vector<shared_ptr<tableEntry> > m_l2_read_req_schedule_q;
    vector<shared_ptr<tableEntry> > m_l2_write_req_schedule_q;
    vector<shared_ptr<tableEntry> > m_ra_req_schedule_q;
    vector<shared_ptr<tableEntry> > m_ra_rep_schedule_q;
    vector<tuple<bool/* is remote */, shared_ptr<void> > > m_dramctrl_req_schedule_q;
    vector<shared_ptr<dramctrlTableEntry> > m_dramctrl_rep_schedule_q;

};

#endif
