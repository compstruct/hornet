// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_PTI_STATS_HPP__
#define __PRIVATE_SHARED_PTI_STATS_HPP__

#include "memStats.hpp"

typedef enum {
    PTI_STAT_TREQ = 0,
    PTI_STAT_PREQ,
    PTI_STAT_RREQ,
    PTI_STAT_INV_REQ,
    PTI_STAT_SWITCH_REQ,
    PTI_STAT_EMPTY_REQ,
    PTI_STAT_DRAMCTRL_READ_REQ,
    PTI_STAT_DRAMCTRL_WRITE_REQ,
    PTI_STAT_NUM_REQ_TYPES
} ptiReqType;

typedef enum {
    PTI_STAT_TREP = 0,
    PTI_STAT_PREP,
    PTI_STAT_RREP,
    PTI_STAT_INV_REP,
    PTI_STAT_SWITCH_REP,
    PTI_STAT_DRAMCTRL_REP,
    PTI_STAT_NUM_REP_TYPES
} ptiRepType;

typedef enum {
    PTI_STAT_LOCAL = 0,
    PTI_STAT_REMOTE,
    PTI_STAT_NUM_SUB_TYPES
} ptiSubType;


class privateSharedPTIStatsPerMemInstr {
public:
    privateSharedPTIStatsPerMemInstr(bool is_read);
    ~privateSharedPTIStatsPerMemInstr();

    inline bool is_read() { return m_is_read; }
    uint64_t total_cost();
    void add(const privateSharedPTIStatsPerMemInstr& other);

    std::shared_ptr<privateSharedPTIStatsPerMemInstr> get_tentative_data(int index);
    int get_max_tentative_data_index();
    inline void clear_tentative_data() { m_tentative_data.clear(); }
    void discard_tentative_data(int index);
    void commit_tentative_data(int index);
    void commit_max_tentative_data();
    void commit_min_tentative_data();

    inline void migration_started(uint64_t depart_time) { m_in_migration = true; m_mig_depart_time = depart_time; }
    void migration_finished(uint64_t arrival_time, bool write_stats);
    inline bool is_in_migration() { return m_in_migration; }

    inline void set_serialization_begin_time_at_current_core(uint64_t begin_time) { m_serialization_begin_time = begin_time; }
    inline uint64_t serialization_begin_time_at_current_core() { return m_serialization_begin_time; }

    inline void core_missed() { m_did_core_miss = true; }

    inline void add_short_latency(uint64_t amnt) { m_short_latency += amnt; }

    inline void add_mem_srz(uint64_t amnt) { m_mem_srz += amnt; }
    inline void add_cat_srz(uint64_t amnt) { m_cat_srz += amnt; }
    inline void add_cat_ops(uint64_t amnt) { m_cat_ops += amnt; }
    inline void add_dram_ops(uint64_t amnt) { m_dram_ops += amnt; }

    inline void add_req_nas(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_req_nas[req][sub] += amnt; }
    inline void add_rep_nas(uint64_t amnt, ptiRepType rep_type, ptiSubType sub) { m_rep_nas[rep_type][sub] += amnt; }

    inline void add_block_cost(uint64_t amnt, ptiReqType req) { m_block_cost[req] += amnt; }
    inline void add_inv_cost(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_inv_cost[req][sub] += amnt; }
    inline void add_reorder_cost(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_reorder_cost[req][sub] += amnt; }
    inline void add_bypass(uint64_t amnt, ptiSubType sub) { m_bypass[sub] += amnt; }

    /* L1 cost categories */
    inline void add_l1_srz(uint64_t amnt) { m_l1_srz += amnt; }
    inline void add_l1_ops(uint64_t amnt) { m_l1_ops += amnt; }
    inline void add_l1_for_read_hit_on_T(uint64_t amnt) { m_l1_for_read_hit_on_T += amnt; }
    inline void add_l1_for_read_hit_on_P(uint64_t amnt) { m_l1_for_read_hit_on_P += amnt; }
    inline void add_l1_for_read_miss_true(uint64_t amnt) { m_l1_for_read_miss_true += amnt; }
    inline void add_l1_for_read_miss_on_expired_T(uint64_t amnt) { m_l1_for_read_miss_on_expired_T += amnt; }
    inline void add_l1_for_write_hit(uint64_t amnt) { m_l1_for_write_hit += amnt; }
    inline void add_l1_for_write_miss_true(uint64_t amnt) { m_l1_for_write_miss_true += amnt; }
    inline void add_l1_for_write_miss_on_T(uint64_t amnt) { m_l1_for_write_miss_on_T += amnt; }
    inline void add_l1_for_feed_for_read(uint64_t amnt) { m_l1_for_feed_for_read += amnt; }
    inline void add_l1_for_feed_for_write(uint64_t amnt) { m_l1_for_feed_for_write += amnt; }

    /* L2 cost categories */
    inline void add_l2_srz(uint64_t amnt) { m_l2_srz += amnt; }
    inline void add_l2_ops(uint64_t amnt) { m_l2_ops += amnt; }
    inline void add_l2_hit_on_valid_T(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_l2_for_hit_on_valid_T[req][sub] += amnt; }
    inline void add_l2_hit_on_expired_T(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_l2_for_hit_on_expired_T[req][sub] += amnt; }
    inline void add_l2_miss_true(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_l2_for_miss_true[req][sub] += amnt; }
    inline void add_l2_miss_on_valid_T(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_l2_for_miss_on_valid_T[req][sub] += amnt; }
    inline void add_l2_miss_on_P(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_l2_for_miss_on_P[req][sub] += amnt; }
    inline void add_l2_miss_reorder(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_l2_for_miss_reorder[req][sub] += amnt; }
    inline void add_l2_for_feed(uint64_t amnt, ptiReqType req, ptiSubType sub) { m_l2_for_feed[req][sub] += amnt; }
    inline void add_l2_for_emptyReq(uint64_t amnt) { m_l2_for_emptyReq += amnt; }

    friend class privateSharedPTIStatsPerTile;
    friend class privateSharedPTIStats;

private:
    bool add_new_tentative_data(int index);

    /* bookmarking */
    bool        m_is_read;
    bool        m_did_core_miss;
    uint64_t    m_serialization_begin_time;
    bool        m_in_migration;
    uint64_t    m_mig_depart_time;

    uint64_t m_inv_cost[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_reorder_cost[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];

    uint64_t m_short_latency;

    uint64_t m_mem_srz;
    uint64_t m_cat_srz;
    uint64_t m_cat_ops;
    uint64_t m_dram_ops;
    uint64_t m_mig;

    uint64_t m_req_nas[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_rep_nas[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_block_cost[PTI_STAT_NUM_REQ_TYPES];
    uint64_t m_bypass[PTI_STAT_NUM_SUB_TYPES];

    uint64_t m_l1_srz;
    uint64_t m_l1_ops;
    uint64_t m_l1_for_read_hit_on_T;
    uint64_t m_l1_for_read_hit_on_P;
    uint64_t m_l1_for_read_miss_on_expired_T;
    uint64_t m_l1_for_read_miss_true;
    uint64_t m_l1_for_write_hit;
    uint64_t m_l1_for_write_miss_true;
    uint64_t m_l1_for_write_miss_on_T;
    uint64_t m_l1_for_feed_for_read;
    uint64_t m_l1_for_feed_for_write;

    uint64_t m_l2_srz;
    uint64_t m_l2_ops;
    uint64_t m_l2_for_hit_on_valid_T[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_l2_for_hit_on_expired_T[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_l2_for_miss_true[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_l2_for_miss_on_valid_T[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_l2_for_miss_on_P[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_l2_for_miss_reorder[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_l2_for_feed[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_l2_for_emptyReq;

    map<int, std::shared_ptr<privateSharedPTIStatsPerMemInstr> > m_tentative_data;

};

class privateSharedPTIStatsPerTile : public memStatsPerTile {
public:
    privateSharedPTIStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~privateSharedPTIStatsPerTile();

    /* instructions */
    inline void new_read_instr_at_l1() { ++m_num_l1_read_instr; }
    inline void new_write_instr_at_l1() { ++m_num_l1_write_instr; }
    inline void new_read_instr_at_l2(ptiSubType sub) { ++m_num_l2_read_instr[sub]; }
    inline void new_write_instr_at_l2(ptiSubType sub) { ++m_num_l2_write_instr[sub]; }
    
    /* caches */
    inline void hit_on_T_for_read_instr_at_l1() { ++m_num_l1_read_hit_on_T; }
    inline void hit_on_P_for_read_instr_at_l1() { ++m_num_l1_read_hit_on_P; }
    inline void miss_true_for_read_instr_at_l1() { ++m_num_l1_read_miss_true; }
    inline void miss_on_expired_T_for_read_instr_at_l1() { ++m_num_l1_read_miss_on_expired_T; }
    inline void hit_for_write_instr_at_l1() { ++m_num_l1_write_hit; }
    inline void miss_true_for_write_instr_at_l1() { ++m_num_l1_write_miss_true; }
    inline void miss_on_T_for_write_instr_at_l1() { ++m_num_l1_write_miss_on_T; }

    inline void hit_on_valid_T_for_read_instr_at_l2(ptiSubType sub) { ++(m_num_l2_read_hit_on_valid_T[sub]); }
    inline void hit_on_expired_T_for_read_instr_at_l2(ptiSubType sub) { ++(m_num_l2_read_hit_on_expired_T[sub]); }
    inline void miss_true_for_read_instr_at_l2(ptiSubType sub) { ++(m_num_l2_read_miss_true[sub]); }
    inline void miss_on_P_for_read_instr_at_l2(ptiSubType sub) { ++(m_num_l2_read_miss_on_P[sub]); }
    inline void miss_reorder_for_read_instr_at_l2(ptiSubType sub) { ++(m_num_l2_read_miss_reorder[sub]); }
    
    inline void hit_on_expired_T_for_write_instr_at_l2(ptiSubType sub) { ++(m_num_l2_write_hit_on_expired_T[sub]); }
    inline void miss_true_for_write_instr_at_l2(ptiSubType sub) { ++(m_num_l2_write_miss_true[sub]); }
    inline void miss_on_valid_T_for_read_instr_at_l2(ptiSubType sub) { ++(m_num_l2_read_miss_on_valid_T[sub]); }
    inline void miss_on_valid_T_for_write_instr_at_l2(ptiSubType sub) { ++(m_num_l2_write_miss_on_valid_T[sub]); }
    inline void miss_on_P_for_write_instr_at_l2(ptiSubType sub) { ++(m_num_l2_write_miss_on_P[sub]); }
    inline void miss_reorder_for_write_instr_at_l2(ptiSubType sub) { ++(m_num_l2_write_miss_reorder[sub]); }

    /* events */

    inline void short_tReq() { ++m_num_short_tReq; }
    /* auto-collect m_num_core_miss_read_instr */
    /* auto-collect m_num_core_miss_write_instr */
    inline void evict_from_l1(ptiSubType sub) { ++(m_num_evict_from_l1[sub]); }
    inline void evict_from_l2() { ++m_num_evict_from_l2; }
    inline void writeback_from_l1(ptiSubType sub) { ++(m_num_writeback_from_l1[sub]); }
    /* auto-collect m_num_block */
    /* auto-collect m_num_inv */
    /* auto-collect m_num_reorder */

    inline void add_cat_action() { ++m_num_cat_action; }
    inline void add_l1_action() { ++m_num_l1_action; }
    inline void add_l2_action() { ++m_num_l2_action; }
    inline void add_dram_action() { ++m_num_dram_action; }

    inline void req_sent(ptiReqType req, ptiSubType sub) { ++(m_req_sent[req][sub]); }
    inline void rep_sent(ptiRepType rep, ptiSubType sub) { ++(m_rep_sent[rep][sub]); }

    void add(const privateSharedPTIStatsPerTile& other);
    void commit_per_mem_instr_stats(const privateSharedPTIStatsPerMemInstr& data);
    inline void commit_per_mem_instr_stats(std::shared_ptr<privateSharedPTIStatsPerMemInstr> data) { commit_per_mem_instr_stats(*data); }

    friend class privateSharedPTIStats;

private:
    /* instructions */
    uint64_t m_num_l1_read_instr;
    uint64_t m_num_l1_write_instr;
    uint64_t m_num_l2_read_instr[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_write_instr[PTI_STAT_NUM_SUB_TYPES];

    /* caches */
    uint64_t m_num_l1_read_hit_on_T;
    uint64_t m_num_l1_read_hit_on_P;
    uint64_t m_num_l1_read_miss_true;
    uint64_t m_num_l1_read_miss_on_expired_T;
    uint64_t m_num_l1_write_hit;
    uint64_t m_num_l1_write_miss_true;
    uint64_t m_num_l1_write_miss_on_T;

    uint64_t m_num_l2_read_hit_on_valid_T[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_read_hit_on_expired_T[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_read_miss_true[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_read_miss_on_P[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_write_miss_on_valid_T[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_read_miss_reorder[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_write_hit_on_expired_T[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_write_miss_true[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_read_miss_on_valid_T[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_write_miss_on_P[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_l2_write_miss_reorder[PTI_STAT_NUM_SUB_TYPES];

    /* events */
    uint64_t m_num_core_miss_read_instr;
    uint64_t m_num_core_miss_write_instr;
    uint64_t m_num_evict_from_l1[PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_evict_from_l2;
    uint64_t m_num_writeback_from_l1[PTI_STAT_NUM_SUB_TYPES];

    uint64_t m_num_block[PTI_STAT_NUM_REQ_TYPES];
    uint64_t m_num_inv[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_num_reorder[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];

    uint64_t m_num_cat_action;
    uint64_t m_num_l1_action;
    uint64_t m_num_l2_action;
    uint64_t m_num_dram_action;
    
    /* messages */
    uint64_t m_req_sent[PTI_STAT_NUM_REQ_TYPES][PTI_STAT_NUM_SUB_TYPES];
    uint64_t m_rep_sent[PTI_STAT_NUM_REP_TYPES][PTI_STAT_NUM_SUB_TYPES];

    uint64_t m_num_short_tReq;

    privateSharedPTIStatsPerMemInstr m_total_per_mem_instr_info;
};

class privateSharedPTIStats : public memStats {
public:
    privateSharedPTIStats(const uint64_t &system_time);
    virtual ~privateSharedPTIStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
