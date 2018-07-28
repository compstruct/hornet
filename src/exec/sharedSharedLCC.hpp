// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SHARED_SHARED_LCC_HPP__
#define __SHARED_SHARED_LCC_HPP__

#include <set>
#include "memory.hpp"
#include "cache.hpp"
#include "cat.hpp"
#include "sharedSharedLCCStats.hpp"

class sharedSharedLCC : public memory {
public:
    typedef enum {
        _REPLACE_LRU = 0,
        _REPLACE_RANDOM = 1,
        _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_LRU = 2,
        _REPLACE_AWAY_EXPIRED_HOME_EVICTABLE_RANDOM = 2,
    } _replacementPolicy_t;

    typedef enum {
        TIMESTAMP_FIXED = 0,
        TIMESTAMP_IDEAL = 1,
        TIMESTAMP_PERIOD_PREDICTION = 2,
        TIMESTAMP_EXTEND_AND_CUT = 3
    } timestampLogic_t;

    typedef enum {
        MIGRATION_NEVER = 0,
        MIGRATION_ALWAYS_ON_WRITES = 1
    } migrationLogic_t;

    typedef struct {
        uint32_t network_width;
        timestampLogic_t timestamp_logic;
        migrationLogic_t migration_logic;
        bool use_checkout_for_write_copy;
        bool use_separate_vc_for_writes;
        uint32_t max_timestamp_delta_for_read_copy;
        uint32_t max_timestamp_delta_for_write_copy;
        uint32_t num_nodes;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
        uint32_t address_size_in_bytes;
        uint32_t num_local_core_ports;
        uint32_t work_table_size_shared;
        uint32_t work_table_size_read_exclusive;
        uint32_t work_table_size_send_checkin_exclusive;
        uint32_t work_table_size_receive_checkin_exclusive;
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
    } sharedSharedLCCCfg_t;

    sharedSharedLCC(uint32_t numeric_id, 
                     const uint64_t &system_time, 
                     std::shared_ptr<tile_statistics> stats, 
                     logger &log, 
                     std::shared_ptr<random_gen> ran, 
                     std::shared_ptr<cat> a_cat, 
                     sharedSharedLCCCfg_t cfg);
    virtual ~sharedSharedLCC();

    virtual uint32_t number_of_mem_msg_types();

    virtual void request(std::shared_ptr<memoryRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* set stats */
    inline void set_per_tile_stats(std::shared_ptr<sharedSharedLCCStatsPerTile> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != std::shared_ptr<sharedSharedLCCStatsPerTile>()); }
    inline std::shared_ptr<sharedSharedLCCStatsPerTile> stats() { return m_stats; }

    typedef enum {
        MSG_DRAMCTRL_REQ = 0,
        MSG_DRAMCTRL_REP,
        MSG_REMOTE_REQ,
        MSG_REMOTE_REP,
        MSG_REMOTE_WRITE_REQ,
        NUM_MSG_TYPES
    } sharedSharedLCCMsgType_t;

    typedef struct {
        
        bool checked_out;
        bool away;

        uint64_t home;
        std::shared_ptr<uint64_t> timestamp;

        /* used for period prediction */
        uint64_t last_remote_read_time;
        uint64_t last_remote_read_period;
        uint64_t last_write_time;
        uint64_t last_write_period;
        uint64_t last_local_access_time;
        uint64_t last_local_access_period;

        /* used for extend and cut */
        bool shared;
        uint32_t first_copyholder;
        uint64_t current_delta;

    } coherenceInfo;

    typedef struct structAuxInfoForCoherence {
        uint32_t current_core;
        uint32_t issued_core;
        bool is_read;
        const sharedSharedLCCCfg_t& cfg;
        structAuxInfoForCoherence(uint32_t cur_core_id, uint32_t issued_core_id, bool read, sharedSharedLCCCfg_t& _cfg) : 
            current_core(cur_core_id), issued_core(issued_core_id), is_read(read), cfg(_cfg) {}

        /* used for extend and cut */
        uint64_t expired_amount;

    } auxInfoForCoherence;

    typedef enum {
        REMOTE_READ_REQ = 0,
        REMOTE_WRITE_REQ,
        REMOTE_REP,
        REMOTE_CHECKIN
    } coherenceMsgType_t;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        coherenceMsgType_t type;
        uint32_t word_count; 
        maddr_t maddr;
        shared_array<uint32_t> data;

        std::shared_ptr<coherenceInfo> coherence_info;

        /* status */
        bool sent;

        /* for extend and cut */
        uint64_t expired_amount;

        /* for stats */
        std::shared_ptr<sharedSharedLCCStatsPerMemInstr> per_mem_instr_stats;
        uint64_t birthtime;

    } coherenceMsg;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        std::shared_ptr<dramRequest> dram_req;
        maddr_t maddr;

        /* status */
        bool sent;

        /* for stats */
        uint64_t birthtime;
        std::shared_ptr<sharedSharedLCCStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlMsg;

private:

    typedef enum {
        _CAT_AND_L1_FOR_LOCAL = 0,
        _L1_FOR_REMOTE,
        _L2,
        _UPDATE_L1,
        _UPDATE_L2,
        _SEND_DRAMCTRL_REQ,
        _WAIT_DRAMCTRL_REP,
        _WRITEBACK_TO_L1,
        _WRITEBACK_TO_L2,
        _WRITEBACK_TO_DRAMCTRL,
        _WAIT_TIMESTAMP,
        _WAIT_CHECKIN,
        _SEND_REMOTE_REQ,
        _SEND_REMOTE_REP,
        _WAIT_REMOTE_REP,
        _SEND_CHECKIN
    } entryStatus;

    typedef enum {
        _WAIT_CHECKIN__L1,
        _WAIT_CHECKIN__L2,
        _WAIT_TIMESTAMP__L1,
        _WAIT_TIMESTAMP__L2,
        _SEND_REMOTE_REP__CACHE,
        _SEND_REMOTE_REP__BYPASS_L1,
        _SEND_REMOTE_REP__BYPASS_L2,
        _SEND_CHECKIN__VOLUNTARY,
        _SEND_CHECKIN__VICTIM,
        _SEND_CHECKIN__UNCACHEABLE,
        _WRITEBACK_TO_DRAMCTRL__FROM_UPDATE_L1,
        _WRITEBACK_TO_DRAMCTRL__FROM_UPDATE_L2,
        _WRITEBACK_TO_L2__FROM_CHECKIN,
        _WRITEBACK_TO_L2__FROM_L1
    } entrySubstatus;

    typedef struct {
        entryStatus status;
        entrySubstatus substatus;

        std::shared_ptr<memoryRequest> core_req;
        std::shared_ptr<catRequest> cat_req;
        std::shared_ptr<cacheRequest> l1_req;
        std::shared_ptr<cacheRequest> l2_req;
        std::shared_ptr<coherenceMsg> remote_req;
        std::shared_ptr<coherenceMsg> bypass_remote_req;
        std::shared_ptr<memoryRequest> bypass_core_req;;
        std::shared_ptr<coherenceMsg> remote_rep;
        std::shared_ptr<coherenceMsg> remote_checkin;
        std::shared_ptr<dramctrlMsg> dramctrl_req;
        std::shared_ptr<dramctrlMsg> dramctrl_rep;

        /* blocking writes and bypassing reads */
        bool can_bypass_read_req; 
        uint64_t blocking_timestamp;
        /* data & info being blocked are stored in the table to serve bypassable reads */
        shared_array<uint32_t> blocked_data;
        std::shared_ptr<coherenceInfo> blocked_line_info;
                                               
        bool using_read_exclusive_space; 
        bool using_send_checkin_exclusive_space; 
        bool using_receive_checkin_exclusive_space; 

        /* for stats */
        uint64_t block_begin_time;
        uint64_t bypass_core_req_begin_time;
        uint64_t bypass_remote_req_begin_time;

        std::shared_ptr<sharedSharedLCCStatsPerMemInstr> per_mem_instr_stats;

    } tableEntry;

    typedef struct {
            std::shared_ptr<dramctrlMsg> dramctrl_req;
            std::shared_ptr<dramctrlMsg> dramctrl_rep;
            std::shared_ptr<message_t> net_msg_to_send;

        /* for stats */
        uint64_t operation_begin_time;
        std::shared_ptr<sharedSharedLCCStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlTableEntry;

    typedef map<maddr_t, std::shared_ptr<tableEntry> > workTable;
    typedef map<maddr_t, std::shared_ptr<dramctrlTableEntry> > dramctrlTable;

    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { 
        maddr.address -= (maddr.address)%(m_cfg.words_per_cache_line*4); return maddr; 
    }

#ifdef HIGH_LATENCY
    inline uint32_t get_flit_count(uint32_t bytes) { return 2*(bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit; }
#else
    inline uint32_t get_flit_count(uint32_t bytes) { return (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit; }
#endif

    /* logics */

    void schedule_requests();
    void update_work_table();
    void update_dramctrl_work_table();
    void accept_incoming_messages();

    /* instance variables */

    sharedSharedLCCCfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    std::shared_ptr<cat> m_cat;

    std::shared_ptr<sharedSharedLCCStatsPerTile> m_stats;

    workTable m_work_table;
    dramctrlTable m_dramctrl_work_table;

    vector<std::tuple<std::shared_ptr<uint64_t>/*timestamp*/, maddr_t/*start_maddr*/> > m_scheduled_checkin;

    uint32_t m_work_table_vacancy_shared;
    uint32_t m_work_table_vacancy_read_exclusive;
    uint32_t m_work_table_vacancy_send_checkin_exclusive;
    uint32_t m_work_table_vacancy_receive_checkin_exclusive;
    uint32_t m_available_core_ports;

    /* Timestamp and other coherence information is usually handled by caches by cache helpers. */
    /* For the cases where the information is updated on the work table not through the cache, */
    /* The work table uses its own helpers */

    typedef std::shared_ptr<void> (*coherenceInfoCopy) (std::shared_ptr<void>);
    coherenceInfoCopy m_copy_coherence_info;
    /* returns true if information is updated (dirty) */
    typedef bool(*coherenceInfoHandler) (std::shared_ptr<coherenceInfo>, std::shared_ptr<auxInfoForCoherence>, const uint64_t&);
    coherenceInfoHandler m_handler_for_dramctrl_rep;
    coherenceInfoHandler m_handler_for_waited_for_timestamp;
    coherenceInfoHandler m_handler_for_remote_rep;
    coherenceInfoHandler m_handler_for_remote_checkin;

    /* keep track of writeback requests to priortize it. must hold other reads and writes until writeback finishes */
    map<maddr_t, std::shared_ptr<tableEntry> > m_l2_writeback_status;
    map<maddr_t, std::shared_ptr<tableEntry> > m_dramctrl_writeback_status;

    /* SCHEDULER QUEUES */
    /* no real hardware... volatile at clock boundaries */

    vector<std::shared_ptr<memoryRequest> > m_core_port_schedule_q; 

    typedef enum {
        FROM_LOCAL_CORE = 0,
        FROM_CHANNEL_REMOTE_REQ,
        FROM_CHANNEL_REMOTE_WRITE_REQ,
        FROM_CHANNEL_REMOTE_REP
    } work_table_entry_src_t;

    vector<std::tuple<work_table_entry_src_t, std::shared_ptr<void> > > m_new_work_table_entry_schedule_q;

    vector<std::shared_ptr<tableEntry> > m_cat_req_schedule_q;
    vector<std::shared_ptr<tableEntry> > m_l1_read_req_schedule_q;
    vector<std::shared_ptr<tableEntry> > m_l1_write_req_schedule_q;
    vector<std::shared_ptr<tableEntry> > m_l2_read_req_schedule_q;
    vector<std::shared_ptr<tableEntry> > m_l2_write_req_schedule_q;
    vector<std::shared_ptr<tableEntry> > m_remote_req_schedule_q;
    vector<std::tuple<bool/* is remote rep*/, std::shared_ptr<tableEntry> > > m_remote_rep_and_checkin_schedule_q;
    vector<std::tuple<bool/* is remote */, std::shared_ptr<void> > > m_dramctrl_req_schedule_q;
    vector<std::shared_ptr<dramctrlTableEntry> > m_dramctrl_rep_schedule_q;

};

#endif
