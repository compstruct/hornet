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
    typedef enum {
        _REPLACE_LRU = 0,
        _REPLACE_RANDOM = 1,
    } _replacementPolicy_t;

    typedef struct {
        bool use_mesi;
        bool use_dir_speculation;
        uint32_t num_nodes;
        uint32_t bytes_per_flit;
        uint32_t words_per_cache_line;
        uint32_t address_size_in_bytes;
        uint32_t num_local_core_ports;
        uint32_t cache_table_size;
        uint32_t dir_table_size_shared;
        uint32_t dir_table_size_cache_rep_exclusive;
        uint32_t dir_table_size_empty_req_exclusive;
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
        MSG_DRAMCTRL_REQ = 0,
        MSG_DRAMCTRL_REP,
        MSG_CACHE_REQ,
        MSG_CACHE_REP,
        MSG_DIR_REQ_REP,
        NUM_MSG_TYPES
    } privateSharedMSIMsgType_t;

    typedef enum {
        EXCLUSIVE = 0,
        SHARED
    } coherenceStatus_t;

    typedef struct {
        coherenceStatus_t status;
        uint32_t home;
    } cacheCoherenceInfo;

    typedef enum {
        LOCAL_READ = 0,
        LOCAL_WRITE,
        UPDATE_BY_SHREP,
        UPDATE_BY_EXREP,
        UPDATE_BY_WBREQ,
        INVALIDATE_BY_INVREQ,
        INVALIDATE_BY_FLUSHREQ
    } cacheAuxInfoForCoherence;

    typedef struct {
        coherenceStatus_t status;
        bool locked;
        set<uint32_t> dir;  
    } dirCoherenceInfo;

    typedef enum {
        SH_REQ = 0,
        EX_REQ,
        INV_REQ,
        EMPTY_REQ,
        WB_REQ,
        FLUSH_REQ,
        SH_REP,
        EX_REP,
        INV_REP,
        WB_REP,
        FLUSH_REP
    } coherenceMsgType_t;

    typedef enum {
        READ_FOR_SHREQ = 0,
        READ_FOR_EXREQ,
        UPDATE_FOR_DRAMFEED,
        UPDATE_FOR_DIR_UPDATE_OR_WRITEBACK,
        UPDATE_FOR_INVREP_OR_FLUSHREP,
        UPDATE_FOR_WBREP,
        UPDATE_FOR_EMPTYREQ
    } dirCacheReqType_t;

    typedef struct {
        uint32_t core_id;
        dirCacheReqType_t req_type;

        /* EMPTY_REQ: info for new cache line (for other maddr) */
        maddr_t replacing_maddr;
        bool empty_req_for_shreq;

        /* feedback */
        shared_ptr<dirCoherenceInfo> initial_dir_info;
        bool is_replaced_line_dirty;
        shared_array<uint32_t> replaced_line;

    } dirAuxInfoForCoherence;

    typedef struct {
        uint32_t sender;
        uint32_t receiver;
        coherenceMsgType_t type;
        uint32_t word_count;
        maddr_t maddr;
        shared_array<uint32_t> data;

        /* status */
        bool sent;

        /* for empty requests */
        uint32_t new_requester;
        maddr_t replacing_maddr;
        bool empty_req_for_shreq;
        bool is_empty_req_done;

        /* for stats */
        shared_ptr<privateSharedMSIStatsPerMemInstr> per_mem_instr_stats;
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
        shared_ptr<privateSharedMSIStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlMsg;

private:

    typedef enum {
        _CACHE_CAT_AND_L1_FOR_LOCAL = 0,
        _CACHE_L1_FOR_DIR_REQ,
        _CACHE_SEND_CACHE_REQ,
        _CACHE_WAIT_DIR_REP,
        _CACHE_UPDATE_L1,
        _CACHE_SEND_CACHE_REP
    } cacheEntryStatus;

    typedef enum {
        _CACHE_SEND_CACHE_REP__SWITCH,
        _CACHE_SEND_CACHE_REP__EVICT,
        _CACHE_SEND_CACHE_REP__DIR_REQ
    } cacheEntrySubstatus;

    typedef struct {
        cacheEntryStatus status;
        cacheEntrySubstatus substatus;

        shared_ptr<memoryRequest> core_req;
        shared_ptr<cacheRequest> l1_req;
        shared_ptr<catRequest> cat_req;
        shared_ptr<coherenceMsg> dir_req;
        shared_ptr<coherenceMsg> dir_rep;
        shared_ptr<coherenceMsg> cache_req;
        shared_ptr<coherenceMsg> cache_rep;

        shared_ptr<privateSharedMSIStatsPerMemInstr> per_mem_instr_stats;
    } cacheTableEntry;

    typedef enum {
        _DIR_L2_FOR_CACHE_REQ = 0,
        _DIR_L2_FOR_EMPTY_REQ,
        _DIR_L2_FOR_CACHE_REP,
        _DIR_SEND_DRAMCTRL_REQ,
        _DIR_WAIT_DRAMCTRL_REP,
        _DIR_UPDATE_L2,
        _DIR_SEND_DRAMCTRL_WRITEBACK,
        _DIR_EMPTY_VICTIM,
        _DIR_SEND_DIR_REP,
        _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP
    } dirEntryStatus;

    typedef enum {
        _DIR_UPDATE_L2__FEED,
        _DIR_UPDATE_L2__DIR_UPDATE_OR_WRITEBACK,
        _DIR_SEND_DRAMCTRL_WRITEBACK__FROM_L2_FEED,
        _DIR_SEND_DRAMCTRL_WRITEBACK__FROM_EVICT,
        _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__REORDER,
        _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__COHERENCE,
        _DIR_SEND_DIR_REQ_AND_WAIT_CACHE_REP__EMPTY_REQ
    } dirEntrySubstatus;

    typedef struct {
        dirEntryStatus status;
        dirEntrySubstatus substatus;

        shared_ptr<coherenceMsg> cache_req;
        shared_ptr<cacheRequest> l2_req;
        shared_ptr<coherenceMsg> cache_rep;
        vector<shared_ptr<coherenceMsg> > dir_reqs;
        shared_ptr<coherenceMsg> dir_rep;
        shared_ptr<dramctrlMsg> dramctrl_req;
        shared_ptr<dramctrlMsg> dramctrl_rep;
        shared_ptr<coherenceMsg> empty_req;

        /* for invalidation */
        dirCoherenceInfo cached_dir;
        shared_array<uint32_t> cached_line;
        bool is_written_back;
        bool need_to_writeback_dir;

        bool using_cache_rep_exclusive_space;
        bool using_empty_req_exclusive_space;

        /* for stats */
        shared_ptr<privateSharedMSIStatsPerMemInstr> per_mem_instr_stats;
    } dirTableEntry;

    typedef struct {
        shared_ptr<dramctrlMsg> dramctrl_req;
        shared_ptr<dramctrlMsg> dramctrl_rep;
        shared_ptr<message_t> net_msg_to_send;

        /* for stats */
        uint64_t operation_begin_time;
        shared_ptr<privateSharedMSIStatsPerMemInstr> per_mem_instr_stats;

    } dramctrlTableEntry;

    typedef map<maddr_t, shared_ptr<cacheTableEntry> > cacheTable;
    typedef map<maddr_t, shared_ptr<dirTableEntry> > dirTable;
    typedef map<maddr_t, shared_ptr<dramctrlTableEntry> > dramctrlTable;

    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { 
        maddr.address -= (maddr.address)%(m_cfg.words_per_cache_line*4); return maddr; 
    }

    inline uint32_t get_flit_count(uint32_t bytes) { return (bytes + m_cfg.bytes_per_flit - 1) / m_cfg.bytes_per_flit; }

    /* logics */

    void schedule_requests();
    void update_cache_table();
    void update_dir_table();
    void update_dramctrl_work_table();
    void accept_incoming_messages();

    /* instance variables */
    
    privateSharedMSICfg_t m_cfg;

    cache* m_l1;
    cache* m_l2;
    shared_ptr<cat> m_cat;

    shared_ptr<privateSharedMSIStatsPerTile> m_stats;

    cacheTable m_cache_table;;
    dirTable m_dir_table;
    dramctrlTable m_dramctrl_work_table;

    uint32_t m_cache_table_vacancy;
    uint32_t m_dir_table_vacancy_shared;
    uint32_t m_dir_table_vacancy_cache_rep_exclusive;
    uint32_t m_dir_table_vacancy_empty_req_exclusive;
    uint32_t m_available_core_ports;

    /* keep track of writeback requests to priortize it. must hold other reads and writes until writeback finishes */
    map<maddr_t, shared_ptr<dirTableEntry> > m_l2_writeback_status;
    map<maddr_t, shared_ptr<dirTableEntry> > m_dramctrl_writeback_status;

    /* SCHEDULER QUEUES */
    /* no real hardware... volatile at clock boundaries */
 
    vector<shared_ptr<memoryRequest> > m_core_port_schedule_q; 

    typedef enum {
        FROM_LOCAL_CORE_REQ = 0,
        FROM_LOCAL_DIR_REQ,
        FROM_REMOTE_DIR_REQ
    } cacheTableEntrySrc_t;

    vector<tuple<cacheTableEntrySrc_t, shared_ptr<void> > > m_new_cache_table_entry_schedule_q;

    vector<tuple<bool/* is remote */, shared_ptr<coherenceMsg> > > m_new_dir_table_entry_for_cache_rep_schedule_q;
    vector<tuple<bool/* is remote */, shared_ptr<coherenceMsg> > > m_new_dir_table_entry_for_req_schedule_q;

    vector<shared_ptr<cacheTableEntry> > m_cat_req_schedule_q;
    vector<shared_ptr<cacheTableEntry> > m_l1_read_req_schedule_q;
    vector<shared_ptr<cacheTableEntry> > m_l1_write_req_schedule_q;
    vector<shared_ptr<cacheTableEntry> > m_cache_req_schedule_q;
    vector<shared_ptr<cacheTableEntry> > m_cache_rep_schedule_q;

    vector<shared_ptr<dirTableEntry> > m_l2_read_req_schedule_q;
    vector<shared_ptr<dirTableEntry> > m_l2_write_req_schedule_q;
    vector<shared_ptr<dirTableEntry> > m_dir_req_schedule_q;
    vector<shared_ptr<dirTableEntry> > m_dir_rep_schedule_q;

    vector<tuple<bool/* is remote */, shared_ptr<void> > > m_dramctrl_req_schedule_q;
    vector<shared_ptr<dramctrlTableEntry> > m_dramctrl_rep_schedule_q;
};

#endif









          

 
