// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_PTI_HPP__
#define __PRIVATE_SHARED_PTI_HPP__

#include <set>
#include "memory.hpp"
#include "cache.hpp"
#include "cat.hpp"
#include "privateSharedPTIStats.hpp"

#define ADDITIONAL_INSTRUMENT
//#undef ADDITIONAL_INSTRUMENT

class privateSharedPTI : public memory {
public:
    typedef enum {
        _REPLACE_LRU = 0,
        _REPLACE_RANDOM = 1,
    } _replacementPolicy_t;

    typedef enum {
        _RENEWAL_IDEAL = 0,
        _RENEWAL_SYNCHED = 1,
        _RENEWAL_SCHEDULED = 2, 
        _RENEWAL_NEVER = 3
    } _renewalType_t;

    typedef enum {
        _ALWAYS = 0,
        _SELECTIVE = 1,
        _NEVER = 2
    } _rRepType_t;

    typedef struct {
        _renewalType_t renewal_type;
        uint32_t delta;
        uint32_t renewal_threshold;
        bool allow_revive;
        bool retry_rReq;
        bool use_rRep_for_tReq;
        bool use_exclusive_vc_for_pReq;
        bool use_exclusive_vc_for_rReq;
        bool use_exclusive_vc_for_rRep;
        _rRepType_t rRep_type;
        uint32_t num_nodes;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
        uint32_t address_size_in_bytes;
        uint32_t num_local_core_ports;
        uint32_t cache_table_size;
        uint32_t dir_table_size_shared;
        uint32_t dir_table_size_cache_rep_exclusive;
        uint32_t dir_table_size_empty_req_exclusive;
        uint32_t cache_renewal_table_size;
        uint32_t dir_renewal_table_size;
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
    } privateSharedPTICfg_t;

    privateSharedPTI(uint32_t numeric_id,
                     const uint64_t &system_time,
                     std::shared_ptr<tile_statistics> stats,
                     logger &log,
                     std::shared_ptr<random_gen> ran,
                     std::shared_ptr<cat> a_cat,
                     privateSharedPTICfg_t cfg);
    virtual ~privateSharedPTI();

    virtual uint32_t number_of_mem_msg_types();

    virtual void request(std::shared_ptr<memoryRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    /* set stats */
    inline void set_per_tile_stats(std::shared_ptr<privateSharedPTIStatsPerTile> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != std::shared_ptr<privateSharedPTIStatsPerTile>()); }
    inline std::shared_ptr<privateSharedPTIStatsPerTile> stats() { return m_stats; }

    typedef enum {
        MSG_DRAMCTRL_REQ = 0,
        MSG_DRAMCTRL_REP,
        MSG_CACHE_REQ,
        MSG_CACHE_PREQ,
        MSG_CACHE_RREQ,
        MSG_CACHE_REP,
        MSG_DIR_REQ_REP,
        MSG_DIR_RREP,
        NUM_MSG_TYPES
    } privateSharedPTIMsgType_t;

    typedef enum {
        TREQ = 0,
        PREQ,
        RREQ,
        INV_REQ,
        SWITCH_REQ,
        EMPTY_REQ,
        TREP,
        PREP,
        RREP,
        INV_REP,
        SWITCH_REP
    } coherenceMsgType_t;

    typedef enum {
        PRIVATE = 0,
        TIMESTAMPED
    } coherenceStatus_t;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        coherenceMsgType_t type;
        uint32_t word_count;
        maddr_t maddr;
        shared_array<uint32_t> data;

        bool sent;

        coherenceStatus_t coherence_status;
        std::shared_ptr<uint64_t> timestamp;

        /* for empty requests */
        maddr_t replacing_maddr;
        std::shared_ptr<void> replacing_info;
        bool is_empty_req_done;

        std::shared_ptr<privateSharedPTIStatsPerMemInstr> per_mem_instr_stats;
        uint64_t birthtime;

        /* to identify tReq-tRep pairs */
        /* or to schedule a future rReq */
        uint64_t requested_time;

    } coherenceMsg;

    typedef struct {
        coherenceStatus_t status;
        uint32_t home;
        std::shared_ptr<uint64_t> timestamp; /* pointer for ideal */

        /* aux stat */
        uint64_t in_time;
        uint64_t last_access;

    } cacheCoherenceInfo;

    typedef enum {
        LOCAL_READ,
        LOCAL_WRITE,
        UPDATE_FOR_DIR_REP,
        UPDATE_FOR_RENEWAL,
        UPDATE_FOR_SWITCH_REQ,
        INVALIDATE_FOR_INV_REQ
    } cacheAuxInfoForCoherence;

    typedef struct {
        bool locked; 
        coherenceStatus_t status;
        uint32_t owner;
        std::shared_ptr<uint64_t> max_timestamp; /* pointer for ideal */
        uint64_t last_write_time;
    } dirCoherenceInfo;

    typedef enum {
        READ_FOR_TREQ = 0,
        READ_FOR_PREQ,
        READ_FOR_RREQ,
        UPDATE_FOR_FEED,
        UPDATE_FOR_EMPTY_REQ,
        UPDATE_FOR_CACHE_REP_ON_NEW,
        UPDATE_FOR_CACHE_REP_FOR_REQ,
        UPDATE_FOR_RREP
    } dirCacheReqType_t;

    typedef struct structDirAuxInfoForCoherence {
        uint32_t core_id;
        dirCacheReqType_t req_type;

        const privateSharedPTICfg_t& cfg;
        structDirAuxInfoForCoherence(privateSharedPTICfg_t& _cfg) : cfg(_cfg) {}

        /* for rReq */
        uint64_t requester_timestamp;
        bool need_to_send_block; /* to return */
        
        /* for empty req */
        maddr_t replacing_maddr;
        bool is_replaced_line_dirty; /* to return */
        shared_array<uint32_t> replaced_line; /* to return */
        std::shared_ptr<dirCoherenceInfo> previous_info; /* to return: record before the current request */

    } dirAuxInfoForCoherence;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        std::shared_ptr<dramRequest> dram_req;
        maddr_t maddr;

        /* status */
        bool sent;

        /* for stats */
        uint64_t birthtime;
        std::shared_ptr<privateSharedPTIStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlMsg;

private:

    typedef enum {
        _CACHE_CAT_AND_L1_FOR_READ = 0, 
        _CACHE_CAT_AND_L1_FOR_WRITE, 
        _CACHE_SEND_TREQ, 
        _CACHE_SEND_PREQ, 
        _CACHE_SEND_RREQ, 
        _CACHE_WAIT_TREP_OR_RREP, 
        _CACHE_WAIT_PREP, 
        _CACHE_UPDATE_L1,
        _CACHE_SEND_CACHE_REP, 
        _CACHE_L1_FOR_DIR_REQ, 
        _CACHE_L1_FOR_RENEWAL
    } cacheEntryStatus;

    typedef struct {
        cacheEntryStatus status;

        std::shared_ptr<memoryRequest> core_req;
        std::shared_ptr<cacheRequest> l1_req;
        std::shared_ptr<catRequest> cat_req;
        std::shared_ptr<coherenceMsg> dir_req;
        std::shared_ptr<coherenceMsg> dir_rep;
        std::shared_ptr<coherenceMsg> cache_req;
        std::shared_ptr<coherenceMsg> cache_rep;

        std::shared_ptr<privateSharedPTIStatsPerMemInstr> per_mem_instr_stats;

        /* stats */
        uint64_t short_latency_begin_time;

    } cacheTableEntry;

    typedef enum {
        _DIR_L2_FOR_PREQ_OR_TREQ = 0, 
        _DIR_L2_FOR_EMPTY_REQ, 
        _DIR_L2_FOR_RREQ,
        _DIR_SEND_DIR_REQ, 
        _DIR_WAIT_CACHE_REP, 
        _DIR_WAIT_TIMESTAMP,
        _DIR_WAIT_DRAMCTRL_REP, 
        _DIR_SEND_DRAMCTRL_REQ, 
        _DIR_SEND_BYPASS_DIR_REP, 
        _DIR_SEND_DIR_REP, 
        _DIR_L2_UPDATE, 
        _DIR_SEND_DRAMCTRL_WRITEBACK, 
        _DIR_SEND_EMPTY_REQ_AND_WAIT 
    } dirEntryStatus;

    typedef enum {
        _DIR_WAIT_CACHE_REP__CACHE_REQ,
        _DIR_WAIT_CACHE_REP__REORDER,
        _DIR_WAIT_CACHE_REP__EMPTY_REQ,
        _DIR_WAIT_TIMESTAMP__CACHE_REQ,
        _DIR_WAIT_TIMESTAMP__EMPTY_REQ,
        _DIR_SEND_DIR_REQ__CACHE_REQ, 
        _DIR_SEND_DIR_REQ__EMPTY_REQ, 
        _DIR_L2_UPDATE__DRAM_REP,
        _DIR_L2_UPDATE__CACHE_REP,
        _DIR_SEND_DIR_REP__RREP, 
        _DIR_SEND_DIR_REP__TREP_PREP, 
        _DIR_SEND_DRAMCTRL_WRITEBACK__FEED,
        _DIR_SEND_DRAMCTRL_WRITEBACK__EVICTION,
    } dirEntrySubstatus;

    typedef struct {
        dirEntryStatus status;
        dirEntrySubstatus substatus;

        std::shared_ptr<coherenceMsg> cache_req;
        std::shared_ptr<coherenceMsg> bypassed_tReq;
        std::shared_ptr<cacheRequest> l2_req;
        std::shared_ptr<coherenceMsg> cache_rep;
        std::shared_ptr<coherenceMsg> dir_req;
        std::shared_ptr<coherenceMsg> dir_rep;
        std::shared_ptr<dramctrlMsg> dramctrl_req;
        std::shared_ptr<dramctrlMsg> dramctrl_rep;
        std::shared_ptr<coherenceMsg> empty_req;

        /* data & info being blocked are stored in the table to serve bypassable reads */
        shared_array<uint32_t> blocked_data;
        std::shared_ptr<dirCoherenceInfo> blocked_line_info;
         
        /* for invalidation */
        dirCoherenceInfo cached_dir;
        shared_array<uint32_t> cached_line;
        bool is_written_back;
        bool need_to_writeback_dir;

        bool using_cache_rep_exclusive_space;
        bool using_empty_req_exclusive_space;

        /* for stats */
        std::shared_ptr<privateSharedPTIStatsPerMemInstr> per_mem_instr_stats;
        uint64_t block_or_inv_begin_time;
        uint64_t bypass_begin_time;
        
    } dirTableEntry;

    typedef struct {
            std::shared_ptr<dramctrlMsg> dramctrl_req;
            std::shared_ptr<dramctrlMsg> dramctrl_rep;
            std::shared_ptr<message_t> net_msg_to_send;

        /* for stats */
        uint64_t operation_begin_time;
        std::shared_ptr<privateSharedPTIStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlTableEntry;

    typedef map<maddr_t, std::shared_ptr<cacheTableEntry> > cacheTable;
    typedef map<maddr_t, std::shared_ptr<dirTableEntry> > dirTable;
    typedef map<maddr_t, std::shared_ptr<dramctrlTableEntry> > dramctrlTable;

    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { 
        maddr.address -= (maddr.address)%(m_cfg.words_per_cache_line*4); return maddr; 
    }

    inline uint32_t get_flit_count(uint32_t bytes) { return (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit; }
    uint32_t get_header_bytes(coherenceMsgType_t type);

    /* logics */
    void schedule_requests();
    void update_cache_table();
    void update_dir_table();
    void update_dramctrl_work_table();
    void accept_incoming_messages();

    privateSharedPTICfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    std::shared_ptr<cat> m_cat;

    std::shared_ptr<privateSharedPTIStatsPerTile> m_stats;

    /* work tables */
    cacheTable m_cache_table;;
    dirTable m_dir_table;
    dramctrlTable m_dramctrl_work_table;


    uint32_t m_cache_table_vacancy;
    uint32_t m_dir_table_vacancy_shared;
    uint32_t m_dir_table_vacancy_cache_rep_exclusive;
    uint32_t m_dir_table_vacancy_empty_req_exclusive;

    uint32_t m_cache_renewal_table_vacancy;
    uint32_t m_dir_renewal_table_vacancy;

    uint32_t m_available_core_ports;

    /* keep track of writeback requests to priortize it. must hold other reads and writes until writeback finishes */
    map<maddr_t, std::shared_ptr<dirTableEntry> > m_l2_writeback_status;
    map<maddr_t, std::shared_ptr<dirTableEntry> > m_dramctrl_writeback_status;

    /* SCHEDULER QUEUES */
    /* no real hardware... volatile at clock boundaries */
 
    vector<std::shared_ptr<memoryRequest> > m_core_port_schedule_q; 

    typedef enum {
        FROM_LOCAL_CORE_REQ = 0,
        FROM_LOCAL_DIR,
        FROM_LOCAL_SCHEDULED_RREQ,
        FROM_REMOTE_DIR,
        FROM_REMOTE_DIR_RREP
    } cacheTableEntrySrc_t;

    vector<std::tuple<cacheTableEntrySrc_t, std::shared_ptr<void> > > m_new_cache_table_entry_schedule_q;

    typedef enum {
        FROM_LOCAL_CACHE,
        FROM_REMOTE_CACHE_REQ,
        FROM_REMOTE_CACHE_PREQ,
        FROM_REMOTE_CACHE_RREQ
    } dirTableEntrySrc_t;

    vector<std::tuple<bool/* is remote */, std::shared_ptr<coherenceMsg> > > m_new_dir_table_entry_for_cache_rep_schedule_q;
    vector<std::tuple<dirTableEntrySrc_t, std::shared_ptr<coherenceMsg> > > m_new_dir_table_entry_for_req_schedule_q;
    vector<std::tuple<dirTableEntrySrc_t, std::shared_ptr<coherenceMsg> > > m_new_dir_table_entry_for_renewal_schedule_q;

    vector<std::shared_ptr<cacheTableEntry> > m_cat_req_schedule_q;

    vector<std::shared_ptr<cacheTableEntry> > m_l1_read_req_schedule_q;
    vector<std::shared_ptr<cacheTableEntry> > m_l1_write_req_schedule_q;
    vector<std::shared_ptr<cacheTableEntry> > m_l1_read_req_schedule_q_for_renewal;
    vector<std::shared_ptr<cacheTableEntry> > m_l1_write_req_schedule_q_for_renewal;

    vector<std::shared_ptr<cacheTableEntry> > m_cache_req_schedule_q;
    vector<std::shared_ptr<cacheTableEntry> > m_cache_rep_schedule_q;

    vector<std::shared_ptr<dirTableEntry> > m_l2_read_req_schedule_q;
    vector<std::shared_ptr<dirTableEntry> > m_l2_write_req_schedule_q;
    vector<std::shared_ptr<dirTableEntry> > m_l2_read_req_schedule_q_for_renewal;
    vector<std::shared_ptr<dirTableEntry> > m_l2_write_req_schedule_q_for_renewal;

    vector<std::shared_ptr<dirTableEntry> > m_dir_req_schedule_q;
    vector<std::shared_ptr<dirTableEntry> > m_dir_rep_schedule_q;

    vector<std::tuple<bool/* is remote */, std::shared_ptr<void> > > m_dramctrl_req_schedule_q;
    vector<std::shared_ptr<dramctrlTableEntry> > m_dramctrl_rep_schedule_q;

    /* scheduled rReq */
    class rReqScheduleQueue{
    public:
        rReqScheduleQueue(bool do_retry, const uint64_t& t);
        ~rReqScheduleQueue();

        void set(std::shared_ptr<coherenceMsg> rReq);
        void remove(maddr_t addr);
        vector<std::shared_ptr<coherenceMsg> > on_due();

    private:
        const uint64_t& system_time;
        bool m_do_retry;

        vector<std::shared_ptr<coherenceMsg> > m_schedule;
        map<maddr_t, std::shared_ptr<coherenceMsg> > m_book;
    };

    rReqScheduleQueue m_rReq_schedule_q;

#ifdef ADDITIONAL_INSTRUMENT
    map<maddr_t, uint32_t> m_renewal_count;
#endif
};

#endif
