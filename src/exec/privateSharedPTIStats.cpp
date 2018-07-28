// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedPTIStats.hpp"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(true) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

privateSharedPTIStatsPerMemInstr::privateSharedPTIStatsPerMemInstr(bool is_read) :
    m_is_read(is_read),
    m_did_core_miss(false),
    m_serialization_begin_time(0),
    m_in_migration(false),
    m_mig_depart_time(0),
    m_short_latency(0),
    m_mem_srz(0),
    m_cat_srz(0),
    m_cat_ops(0),
    m_dram_ops(0),
    m_mig(0),
    m_l1_srz(0),
    m_l1_ops(0),
    m_l1_for_read_hit_on_T(0),
    m_l1_for_read_hit_on_P(0),
    m_l1_for_read_miss_on_expired_T(0),
    m_l1_for_read_miss_true(0),
    m_l1_for_write_hit(0),
    m_l1_for_write_miss_true(0),
    m_l1_for_write_miss_on_T(0),
    m_l1_for_feed_for_read(0),
    m_l1_for_feed_for_write(0),
    m_l2_srz(0),
    m_l2_ops(0),
    m_l2_for_emptyReq(0)
{
    memset(m_inv_cost, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_reorder_cost, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_req_nas, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_rep_nas, 0, PTI_STAT_NUM_REP_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_block_cost, 0, PTI_STAT_NUM_REQ_TYPES * sizeof(uint64_t));
    memset(m_bypass, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_l2_for_hit_on_valid_T, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_l2_for_hit_on_expired_T, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_l2_for_miss_true, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_l2_for_miss_on_valid_T, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_l2_for_miss_on_P, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_l2_for_miss_reorder, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_l2_for_feed, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
}

privateSharedPTIStatsPerMemInstr::~privateSharedPTIStatsPerMemInstr() {}

void privateSharedPTIStatsPerMemInstr::add(const privateSharedPTIStatsPerMemInstr& other) {
    m_short_latency += other.m_short_latency;
    m_mem_srz += other.m_mem_srz;
    m_cat_srz += other.m_cat_srz;
    m_cat_ops += other.m_cat_ops;
    m_dram_ops += other.m_dram_ops;
    m_mig += other.m_mig;
    m_l1_srz+= other.m_l1_srz;
    m_l1_ops+= other.m_l1_ops;
    m_l1_for_read_hit_on_T += other.m_l1_for_read_hit_on_T;
    m_l1_for_read_hit_on_P += other.m_l1_for_read_hit_on_P;
    m_l1_for_read_miss_on_expired_T += other.m_l1_for_read_miss_on_expired_T;
    m_l1_for_read_miss_true += other.m_l1_for_read_miss_true;
    m_l1_for_write_hit += other.m_l1_for_write_hit;
    m_l1_for_write_miss_true += other.m_l1_for_write_miss_true;
    m_l1_for_write_miss_on_T += other.m_l1_for_write_miss_on_T;
    m_l1_for_feed_for_read += other.m_l1_for_feed_for_read;
    m_l1_for_feed_for_write += other.m_l1_for_feed_for_write;
    m_l2_srz += other.m_l2_srz;
    m_l2_ops += other.m_l2_ops;
    m_l2_for_emptyReq += other.m_l2_for_emptyReq;

    for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
        m_block_cost[i] += other.m_block_cost[i];
    }

    for (int i = 0; i < PTI_STAT_NUM_SUB_TYPES; ++i) {
        m_bypass[i] += other.m_bypass[i];
    }

    for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            m_inv_cost[i][j] += other.m_inv_cost[i][j];
            m_reorder_cost[i][j] += other.m_reorder_cost[i][j];
            m_req_nas[i][j] += other.m_req_nas[i][j];
            m_rep_nas[i][j] += other.m_rep_nas[i][j];
            m_l2_for_hit_on_valid_T[i][j] += other.m_l2_for_hit_on_valid_T[i][j];
            m_l2_for_hit_on_expired_T[i][j] += other.m_l2_for_hit_on_expired_T[i][j];
            m_l2_for_miss_true[i][j] += other.m_l2_for_miss_true[i][j];
            m_l2_for_miss_on_valid_T[i][j] += other.m_l2_for_miss_on_valid_T[i][j];
            m_l2_for_miss_on_P[i][j] += other.m_l2_for_miss_on_P[i][j];
            m_l2_for_miss_reorder[i][j] += other.m_l2_for_miss_reorder[i][j];
            m_l2_for_feed[i][j] += other.m_l2_for_feed[i][j];
        }
    }
}

uint64_t privateSharedPTIStatsPerMemInstr::total_cost() {
    uint64_t total = m_short_latency + m_mem_srz + m_cat_srz + m_cat_ops + m_dram_ops + m_mig 
                     + m_l1_srz + m_l1_ops + m_l2_srz + m_l2_ops;

    for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
        total += m_block_cost[i];
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            total += m_inv_cost[i][j];
            total += m_reorder_cost[i][j];
            total += m_req_nas[i][j];
        }
    }
    for (int i = 0; i < PTI_STAT_NUM_REP_TYPES; ++i) {
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            total += m_rep_nas[i][j];
        }
    }
    for (int i = 0; i < PTI_STAT_NUM_SUB_TYPES; ++i) {
        total += m_bypass[i];
    }
    return total;
}

bool privateSharedPTIStatsPerMemInstr::add_new_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        return false;
    }
    std::shared_ptr<privateSharedPTIStatsPerMemInstr> new_tentative_set(new privateSharedPTIStatsPerMemInstr(m_is_read));
    m_tentative_data[index] = new_tentative_set;
    return true;
}

std::shared_ptr<privateSharedPTIStatsPerMemInstr> privateSharedPTIStatsPerMemInstr::get_tentative_data(int index) {
    if (!m_tentative_data.count(index)) {
        add_new_tentative_data(index);
    }
    return m_tentative_data[index];
}

int privateSharedPTIStatsPerMemInstr::get_max_tentative_data_index() {
    mh_assert(m_tentative_data.size());
    uint64_t value = 0;
    map<int, std::shared_ptr<privateSharedPTIStatsPerMemInstr> >::iterator it;
    int max = 0;
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
            std::shared_ptr<privateSharedPTIStatsPerMemInstr> cur = it->second;
        if (cur->total_cost() > value) {
            value = cur->total_cost();
            max = it->first;
        }
    }

    return max;
}

void privateSharedPTIStatsPerMemInstr::commit_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        add(*m_tentative_data[index]);
    }
    m_tentative_data.clear();
}

void privateSharedPTIStatsPerMemInstr::commit_max_tentative_data() {
    if (m_tentative_data.size() == 0) {
        return;
    }

    add(*m_tentative_data[get_max_tentative_data_index()]);
    m_tentative_data.clear();
}

void privateSharedPTIStatsPerMemInstr::commit_min_tentative_data() {
    uint64_t value = UINT64_MAX;
    map<int, std::shared_ptr<privateSharedPTIStatsPerMemInstr> >::iterator it;
    std::shared_ptr<privateSharedPTIStatsPerMemInstr> min = std::shared_ptr<privateSharedPTIStatsPerMemInstr>();
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
            std::shared_ptr<privateSharedPTIStatsPerMemInstr> cur = it->second;
        if (cur->total_cost() < value) {
            value = cur->total_cost();
            min = cur;
        }
    }
    if (min) {
        add(*min);
        m_tentative_data.clear();
    }
}

void privateSharedPTIStatsPerMemInstr::migration_finished(uint64_t arrival_time, bool write_stats) {
    if (m_in_migration) {
        if (write_stats) {
            m_mig += arrival_time - m_mig_depart_time;
        }
        m_in_migration = false;
    }
}

privateSharedPTIStatsPerTile::privateSharedPTIStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t), 
    m_num_l1_read_instr(0),
    m_num_l1_write_instr(0),
    m_num_l1_read_hit_on_T(0),
    m_num_l1_read_hit_on_P(0),
    m_num_l1_read_miss_true(0),
    m_num_l1_read_miss_on_expired_T(0),
    m_num_l1_write_hit(0),
    m_num_l1_write_miss_true(0),
    m_num_l1_write_miss_on_T(0),
    m_num_core_miss_read_instr(0),
    m_num_core_miss_write_instr(0),
    m_num_evict_from_l2(0),
    m_num_cat_action(0),
    m_num_l1_action(0),
    m_num_l2_action(0),
    m_num_dram_action(0),
    m_num_short_tReq(0),
    m_total_per_mem_instr_info(false/*don't care*/)
{
    memset(m_num_l2_read_instr, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_write_instr, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_reorder, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_req_sent, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_rep_sent, 0, PTI_STAT_NUM_REP_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_writeback_from_l1, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_evict_from_l1, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_block, 0, PTI_STAT_NUM_REQ_TYPES * sizeof(uint64_t));
    memset(m_num_inv, 0, PTI_STAT_NUM_REQ_TYPES * PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_read_hit_on_valid_T, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_read_hit_on_expired_T, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_read_miss_true, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_read_miss_on_P, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_read_miss_on_valid_T, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_read_miss_reorder, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_write_hit_on_expired_T, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_write_miss_true, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_write_miss_on_valid_T, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_write_miss_on_P, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
    memset(m_num_l2_write_miss_reorder, 0, PTI_STAT_NUM_SUB_TYPES * sizeof(uint64_t));
}

privateSharedPTIStatsPerTile::~privateSharedPTIStatsPerTile() {}

void privateSharedPTIStatsPerTile::add(const privateSharedPTIStatsPerTile& other) {
    m_num_l1_read_instr += other.m_num_l1_read_instr;
    m_num_l1_write_instr += other.m_num_l1_write_instr;
    m_num_l1_read_hit_on_T += other.m_num_l1_read_hit_on_T;
    m_num_l1_read_hit_on_P += other.m_num_l1_read_hit_on_P;
    m_num_l1_read_miss_true += other.m_num_l1_read_miss_true;
    m_num_l1_read_miss_on_expired_T += other.m_num_l1_read_miss_on_expired_T;
    m_num_l1_write_hit += other.m_num_l1_write_hit;
    m_num_l1_write_miss_true += other.m_num_l1_write_miss_true;
    m_num_l1_write_miss_on_T += other.m_num_l1_write_miss_on_T;
    m_num_core_miss_read_instr += other.m_num_core_miss_read_instr;
    m_num_core_miss_write_instr += other.m_num_core_miss_write_instr;
    m_num_evict_from_l2 += other.m_num_evict_from_l2;
    m_num_cat_action += other.m_num_cat_action;
    m_num_l1_action += other.m_num_l1_action;
    m_num_l2_action += other.m_num_l2_action;
    m_num_dram_action += other.m_num_dram_action;
    m_num_short_tReq += other.m_num_short_tReq;
    
    for (int i = 0; i < PTI_STAT_NUM_SUB_TYPES; ++i) {
        m_num_l2_read_instr[i] += other.m_num_l2_read_instr[i];
        m_num_l2_write_instr[i] += other.m_num_l2_write_instr[i];
        m_num_writeback_from_l1[i] += other.m_num_writeback_from_l1[i];
        m_num_evict_from_l1[i] += other.m_num_evict_from_l1[i];
        m_num_l2_read_hit_on_valid_T[i] += other.m_num_l2_read_hit_on_valid_T[i];
        m_num_l2_read_hit_on_expired_T[i] += other.m_num_l2_read_hit_on_expired_T[i];
        m_num_l2_read_miss_true[i] += other.m_num_l2_read_miss_true[i];
        m_num_l2_read_miss_on_P[i] += other.m_num_l2_read_miss_on_P[i];
        m_num_l2_read_miss_on_valid_T[i] += other.m_num_l2_read_miss_on_valid_T[i];
        m_num_l2_read_miss_reorder[i] += other.m_num_l2_read_miss_reorder[i];
        m_num_l2_write_hit_on_expired_T[i] += other.m_num_l2_write_hit_on_expired_T[i];
        m_num_l2_write_miss_true[i] += other.m_num_l2_write_miss_true[i];
        m_num_l2_write_miss_on_valid_T[i] += other.m_num_l2_write_miss_on_valid_T[i];
        m_num_l2_write_miss_on_P[i] += other.m_num_l2_write_miss_on_P[i];
        m_num_l2_write_miss_reorder[i] += other.m_num_l2_write_miss_reorder[i];
    }


    for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
        m_num_block[i] += other.m_num_block[i];
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            m_num_reorder[i][j] += other.m_num_reorder[i][j];
            m_req_sent[i][j] += other.m_req_sent[i][j];
            m_num_inv[i][j] += other.m_num_inv[i][j];
        }
    }

    for (int i = 0; i < PTI_STAT_NUM_REP_TYPES; ++i) {
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            m_rep_sent[i][j] += other.m_rep_sent[i][j];
        }
    }




        m_total_per_mem_instr_info.add(other.m_total_per_mem_instr_info);
}

void privateSharedPTIStatsPerTile::commit_per_mem_instr_stats(const privateSharedPTIStatsPerMemInstr& data) {

    /* auto-collect m_num_core_miss_read_instr */
    /* auto-collect m_num_core_miss_write_instr */
    if (data.m_did_core_miss) {
        if (data.m_is_read) {
            ++m_num_core_miss_read_instr;
        } else {
            ++m_num_core_miss_write_instr;
        }
    }

    /* auto-collect */
    for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
        if (data.m_block_cost[i]) {
            ++(m_num_block[i]);
        }
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            if (data.m_inv_cost[i][j]) {
                ++(m_num_inv[i][j]);
            }
            if (data.m_reorder_cost[i][j]) {
                ++(m_num_reorder[i][j]);
            }
        }
    }

    m_total_per_mem_instr_info.add(data);
}

privateSharedPTIStats::privateSharedPTIStats(const uint64_t &t) : memStats(t) {}

privateSharedPTIStats::~privateSharedPTIStats() {}

void privateSharedPTIStats::print_stats(ostream &out) {
    
    char str[4096];
    privateSharedPTIStatsPerTile t(0, system_time);

    memStats::print_stats(out);

    /* add privateSharedPTI-specific statistics */

    out << dec << endl;
    
    out << "Private-L1 Shared-L2 PTI Statistics" << endl;
    out << "----------------------------------------" << endl;
    out << endl;

    perTileStats_t::iterator it;

    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
            std::shared_ptr<privateSharedPTIStatsPerTile> st = static_pointer_cast<privateSharedPTIStatsPerTile>(it->second);
        t.add(*st);
    }

    out << endl;

    sprintf(str, "[P1S2PTI:Summary] \n"
                 "    -- L1 -- \n"
                 "    L1-read-instr %ld\n"
                 "    L1-read-instr-hits-on-T %ld\n"
                 "    L1-read-instr-hits-on-P %ld\n"
                 "    L1-read-instr-miss-true %ld\n"
                 "    L1-read-instr-miss-on-expired-T %ld\n"
                 "    L1-write-instr %ld\n"
                 "    L1-write-instr-hits %ld\n"
                 "    L1-write-instr-miss-true %ld\n"
                 "    L1-write-instr-miss-on-T %ld\n"
                 "    -- L2 -- \n"
                 "    L2-read_instr %ld (local: %ld remote: %ld )\n"
                 "    L2-read-instr-hits-on-valid-T %ld (local: %ld remote: %ld )\n"
                 "    L2-read-instr-hits-on-expired-T %ld (local: %ld remote: %ld )\n"
                 "    L2-read-instr-miss-true %ld (local: %ld remote: %ld )\n"
                 "    L2-read-instr-miss-on-P %ld (local: %ld remote: %ld )\n"
                 "    L2-read-instr-miss-on-valid-T %ld (local: %ld remote: %ld )\n"
                 "    L2-read-instr-miss-reorder %ld (local: %ld remote: %ld )\n"
                 "    L2-write_instr %ld (local: %ld remote: %ld )\n"
                 "    L2-write-instr-hits-on-expired-T %ld (local: %ld remote: %ld )\n"
                 "    L2-write-instr-miss-true %ld (local: %ld remote: %ld )\n"
                 "    L2-write-instr-miss-on-valid-T %ld (local: %ld remote: %ld )\n"
                 "    L2-write-instr-miss-on-P-T %ld (local: %ld remote: %ld )\n"
                 "    L2-write-instr-miss-reorder %ld (local: %ld remote: %ld )\n"
                 "    -- Messages --\n"
                 "    tReq %ld (local: %ld remote: %ld )\n"
                 "    tRep %ld (local: %ld remote: %ld )\n"
                 "    pReq %ld (local: %ld remote: %ld )\n"
                 "    pRep %ld (local: %ld remote: %ld )\n"
                 "    rReq %ld (local: %ld remote: %ld )\n"
                 "    rRep %ld (local: %ld remote: %ld )\n"
                 "    invRep %ld (local: %ld remote: %ld )\n"
                 "    switchRep %ld (local: %ld remote: %ld )\n"
                 "    emptyReq %ld\n"
                 "    -- Events --\n"
                 "    core-missed-read-instr %ld\n"
                 "    core-missed-write-instr %ld\n"
                 "    invalidation-for-tReq %ld (local: %ld remote: %ld )\n"
                 "    invalidation-for-pReq %ld (local: %ld remote: %ld )\n"
                 "    invalidation-for-emptyReq %ld (local: %ld remote: %ld )\n"
                 "    timestamp-block-for-pReq %ld \n"
                 "    timestamp-block-for-emptyReq %ld \n"
                 "    evict-from-L1 %ld (local: %ld remote: %ld )\n"
                 "    evict-from-L2 %ld\n"
                 "    writeback-from-L1 %ld (local: %ld remote: %ld )\n"
                 "    CAT-actions %ld\n"
                 "    L1-actions %ld\n"
                 "    L2-actions %ld\n"
                 "    DRAM-actions %ld\n"
                 "    Shortened tReq-tRep trip %ld\n",
                 t.m_num_l1_read_instr,
                 t.m_num_l1_read_hit_on_T,
                 t.m_num_l1_read_hit_on_P,
                 t.m_num_l1_read_miss_true,
                 t.m_num_l1_read_miss_on_expired_T,
                 t.m_num_l1_write_instr,
                 t.m_num_l1_write_hit,
                 t.m_num_l1_write_miss_true,
                 t.m_num_l1_write_miss_on_T,
                 t.m_num_l2_read_instr[PTI_STAT_LOCAL] + t.m_num_l2_read_instr[PTI_STAT_REMOTE], 
                     t.m_num_l2_read_instr[PTI_STAT_LOCAL], 
                     t.m_num_l2_read_instr[PTI_STAT_REMOTE],
                 t.m_num_l2_read_hit_on_valid_T[PTI_STAT_LOCAL] + t.m_num_l2_read_hit_on_valid_T[PTI_STAT_REMOTE], 
                     t.m_num_l2_read_hit_on_valid_T[PTI_STAT_LOCAL], 
                     t.m_num_l2_read_hit_on_valid_T[PTI_STAT_REMOTE],
                 t.m_num_l2_read_hit_on_expired_T[PTI_STAT_LOCAL] + t.m_num_l2_read_hit_on_expired_T[PTI_STAT_REMOTE], 
                     t.m_num_l2_read_hit_on_expired_T[PTI_STAT_LOCAL], 
                     t.m_num_l2_read_hit_on_expired_T[PTI_STAT_REMOTE],
                 t.m_num_l2_read_miss_true[PTI_STAT_LOCAL] + t.m_num_l2_read_miss_true[PTI_STAT_REMOTE], 
                     t.m_num_l2_read_miss_true[PTI_STAT_LOCAL], 
                     t.m_num_l2_read_miss_true[PTI_STAT_REMOTE],
                 t.m_num_l2_read_miss_on_P[PTI_STAT_LOCAL] + t.m_num_l2_read_miss_on_P[PTI_STAT_REMOTE], 
                     t.m_num_l2_read_miss_on_P[PTI_STAT_LOCAL], 
                     t.m_num_l2_read_miss_on_P[PTI_STAT_REMOTE],
                 t.m_num_l2_read_miss_on_valid_T[PTI_STAT_LOCAL] + t.m_num_l2_read_miss_on_valid_T[PTI_STAT_REMOTE], 
                     t.m_num_l2_read_miss_on_valid_T[PTI_STAT_LOCAL], 
                     t.m_num_l2_read_miss_on_valid_T[PTI_STAT_REMOTE],
                 t.m_num_l2_read_miss_reorder[PTI_STAT_LOCAL] + t.m_num_l2_read_miss_reorder[PTI_STAT_REMOTE], 
                     t.m_num_l2_read_miss_reorder[PTI_STAT_LOCAL], 
                     t.m_num_l2_read_miss_reorder[PTI_STAT_REMOTE],
                 t.m_num_l2_write_instr[PTI_STAT_LOCAL] + t.m_num_l2_write_instr[PTI_STAT_REMOTE], 
                     t.m_num_l2_write_instr[PTI_STAT_LOCAL], 
                     t.m_num_l2_write_instr[PTI_STAT_REMOTE],
                 t.m_num_l2_write_hit_on_expired_T[PTI_STAT_LOCAL] + t.m_num_l2_write_hit_on_expired_T[PTI_STAT_REMOTE], 
                     t.m_num_l2_write_hit_on_expired_T[PTI_STAT_LOCAL], 
                     t.m_num_l2_write_hit_on_expired_T[PTI_STAT_REMOTE],
                 t.m_num_l2_write_miss_true[PTI_STAT_LOCAL] + t.m_num_l2_write_miss_true[PTI_STAT_REMOTE], 
                     t.m_num_l2_write_miss_true[PTI_STAT_LOCAL], 
                     t.m_num_l2_write_miss_true[PTI_STAT_REMOTE],
                 t.m_num_l2_write_miss_on_valid_T[PTI_STAT_LOCAL] + t.m_num_l2_write_miss_on_valid_T[PTI_STAT_REMOTE], 
                     t.m_num_l2_write_miss_on_valid_T[PTI_STAT_LOCAL], 
                     t.m_num_l2_write_miss_on_valid_T[PTI_STAT_REMOTE],
                 t.m_num_l2_write_miss_on_P[PTI_STAT_LOCAL] + t.m_num_l2_write_miss_on_P[PTI_STAT_REMOTE], 
                     t.m_num_l2_write_miss_on_P[PTI_STAT_LOCAL], 
                     t.m_num_l2_write_miss_on_P[PTI_STAT_REMOTE],
                 t.m_num_l2_write_miss_reorder[PTI_STAT_LOCAL] + t.m_num_l2_write_miss_reorder[PTI_STAT_REMOTE], 
                     t.m_num_l2_write_miss_reorder[PTI_STAT_LOCAL], 
                     t.m_num_l2_write_miss_reorder[PTI_STAT_REMOTE],
                 t.m_req_sent[PTI_STAT_TREQ][PTI_STAT_LOCAL] + t.m_req_sent[PTI_STAT_TREQ][PTI_STAT_REMOTE],
                     t.m_req_sent[PTI_STAT_TREQ][PTI_STAT_LOCAL],
                     t.m_req_sent[PTI_STAT_TREQ][PTI_STAT_REMOTE],
                 t.m_rep_sent[PTI_STAT_TREP][PTI_STAT_LOCAL] + t.m_rep_sent[PTI_STAT_TREP][PTI_STAT_REMOTE],
                     t.m_rep_sent[PTI_STAT_TREP][PTI_STAT_LOCAL],
                     t.m_rep_sent[PTI_STAT_TREP][PTI_STAT_REMOTE],
                 t.m_req_sent[PTI_STAT_PREQ][PTI_STAT_LOCAL] + t.m_req_sent[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                     t.m_req_sent[PTI_STAT_PREQ][PTI_STAT_LOCAL],
                     t.m_req_sent[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                 t.m_rep_sent[PTI_STAT_PREP][PTI_STAT_LOCAL] + t.m_rep_sent[PTI_STAT_PREP][PTI_STAT_REMOTE],
                     t.m_rep_sent[PTI_STAT_PREP][PTI_STAT_LOCAL],
                     t.m_rep_sent[PTI_STAT_PREP][PTI_STAT_REMOTE],
                 t.m_req_sent[PTI_STAT_RREQ][PTI_STAT_LOCAL] + t.m_req_sent[PTI_STAT_RREQ][PTI_STAT_REMOTE],
                     t.m_req_sent[PTI_STAT_RREQ][PTI_STAT_LOCAL],
                     t.m_req_sent[PTI_STAT_RREQ][PTI_STAT_REMOTE],
                 t.m_rep_sent[PTI_STAT_RREP][PTI_STAT_LOCAL] + t.m_rep_sent[PTI_STAT_RREP][PTI_STAT_REMOTE],
                     t.m_rep_sent[PTI_STAT_RREP][PTI_STAT_LOCAL],
                     t.m_rep_sent[PTI_STAT_RREP][PTI_STAT_REMOTE],
                 t.m_rep_sent[PTI_STAT_INV_REP][PTI_STAT_LOCAL] + t.m_rep_sent[PTI_STAT_INV_REP][PTI_STAT_REMOTE],
                     t.m_rep_sent[PTI_STAT_INV_REP][PTI_STAT_LOCAL],
                     t.m_rep_sent[PTI_STAT_INV_REP][PTI_STAT_REMOTE],
                 t.m_rep_sent[PTI_STAT_SWITCH_REP][PTI_STAT_LOCAL] + t.m_rep_sent[PTI_STAT_SWITCH_REP][PTI_STAT_REMOTE],
                     t.m_rep_sent[PTI_STAT_SWITCH_REP][PTI_STAT_LOCAL],
                     t.m_rep_sent[PTI_STAT_SWITCH_REP][PTI_STAT_REMOTE],
                 t.m_req_sent[PTI_STAT_EMPTY_REQ][PTI_STAT_LOCAL],
                 t.m_num_core_miss_read_instr,
                 t.m_num_core_miss_write_instr,
                 t.m_num_inv[PTI_STAT_TREQ][PTI_STAT_LOCAL] + t.m_num_inv[PTI_STAT_TREQ][PTI_STAT_REMOTE],
                     t.m_num_inv[PTI_STAT_TREQ][PTI_STAT_LOCAL],
                     t.m_num_inv[PTI_STAT_TREQ][PTI_STAT_REMOTE],
                 t.m_num_inv[PTI_STAT_PREQ][PTI_STAT_LOCAL] + t.m_num_inv[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                     t.m_num_inv[PTI_STAT_PREQ][PTI_STAT_LOCAL],
                     t.m_num_inv[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                 t.m_num_inv[PTI_STAT_EMPTY_REQ][PTI_STAT_LOCAL] + t.m_num_inv[PTI_STAT_EMPTY_REQ][PTI_STAT_REMOTE],
                     t.m_num_inv[PTI_STAT_EMPTY_REQ][PTI_STAT_LOCAL],
                     t.m_num_inv[PTI_STAT_EMPTY_REQ][PTI_STAT_REMOTE],
                 t.m_num_block[PTI_STAT_PREQ],
                 t.m_num_block[PTI_STAT_EMPTY_REQ],
                 t.m_num_evict_from_l1[PTI_STAT_LOCAL] + t.m_num_evict_from_l1[PTI_STAT_REMOTE],
                     t.m_num_evict_from_l1[PTI_STAT_LOCAL],
                     t.m_num_evict_from_l1[PTI_STAT_REMOTE],
                 t.m_num_evict_from_l2,
                 t.m_num_writeback_from_l1[PTI_STAT_LOCAL] + t.m_num_writeback_from_l1[PTI_STAT_REMOTE],
                     t.m_num_writeback_from_l1[PTI_STAT_LOCAL],
                     t.m_num_writeback_from_l1[PTI_STAT_REMOTE],
                 t.m_num_cat_action,
                 t.m_num_l1_action,
                 t.m_num_l2_action,
                 t.m_num_dram_action,
                 t.m_num_short_tReq);
                 
    out << str << endl;

    {
        privateSharedPTIStatsPerMemInstr &x = t.m_total_per_mem_instr_info;
        sprintf(str, "[P1S2PTI Latency Breakdown ]\n"
                     "    migration %ld\n"
                     "    memory-serialization %ld\n"
                     "    outstanding-CAT-serialization %ld\n"
                     "    outstanding-CAT-operation %ld\n"
                     "    outstanding-L1-operation-hit %ld\n"
                     "    outstanding-L1-operation-miss %ld\n"
                     "    outstanding-L1-operation-update %ld\n"
                     "    shortened-reply-latency %ld\n"
                     "    outstanding-tReq-network-and-serialization %ld\n"
                     "    pReq-network-and-serialization %ld\n"
                     "    outstanding-invRep-network-and-serialization %ld\n"
                     "    bypass-latency %ld\n"
                     "    outstanding-L2-operation-hit %ld\n"
                     "    outstanding-L2-operation-miss %ld\n"
                     "    outstanding-L2-operation-update %ld\n"
                     "    outstanding-reorder-latency %ld\n"
                     "    timestamp-blocked-delay %ld\n"
                     "    outstanding-inv-latency-for-tReq %ld\n"
                     "    outstanding-inv-latency-for-pReq %ld\n"
                     "    emptyreq-serialization %ld\n"
                     "    outstanding-L2-operation-evict %ld\n"
                     "    outstanding-inv-latency-for-evict %ld\n"
                     "    outstanding-timestamp-blocked-latency-for-evict %ld\n"
                     "    dram-feed-req-network-and-serialization %ld\n"
                     "    outstanding-dram-writeback-req-network-and-serialization %ld\n"
                     "    outstanding-DRAM-operation %ld\n"
                     "    dram-feed-reply-network-and-serialization %ld\n"
                     "    tRep-network-and-serialization %ld\n"
                     "    pRep-network-and-serialization %ld\n"
                     "    outstanding-rReq-network-and-serialization %ld\n",
                x.m_mig,
                x.m_mem_srz,
                x.m_cat_srz,
                x.m_cat_ops,
                x.m_l1_for_read_hit_on_T + x.m_l1_for_read_hit_on_P + x.m_l1_for_write_hit,
                x.m_l1_for_read_miss_on_expired_T + x.m_l1_for_read_miss_true + x.m_l1_for_write_miss_true + x.m_l1_for_write_miss_on_T,
                x.m_l1_for_feed_for_read + x.m_l1_for_feed_for_write,
                x.m_short_latency,
                x.m_req_nas[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_req_nas[PTI_STAT_TREQ][PTI_STAT_REMOTE],
                x.m_req_nas[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_req_nas[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                x.m_rep_nas[PTI_STAT_INV_REP][PTI_STAT_LOCAL] + x.m_rep_nas[PTI_STAT_INV_REP][PTI_STAT_REMOTE],
                x.m_bypass[PTI_STAT_LOCAL] + x.m_bypass[PTI_STAT_REMOTE],
                x.m_l2_for_hit_on_valid_T[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_l2_for_hit_on_valid_T[PTI_STAT_TREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_hit_on_expired_T[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_l2_for_hit_on_expired_T[PTI_STAT_TREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_hit_on_expired_T[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_l2_for_hit_on_expired_T[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                x.m_l2_for_miss_true[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_l2_for_miss_true[PTI_STAT_TREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_miss_true[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_l2_for_miss_true[PTI_STAT_PREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_miss_on_valid_T[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_l2_for_miss_on_valid_T[PTI_STAT_PREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_miss_on_P[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_l2_for_miss_on_P[PTI_STAT_TREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_miss_on_P[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_l2_for_miss_on_P[PTI_STAT_PREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_miss_reorder[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_l2_for_miss_reorder[PTI_STAT_TREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_miss_reorder[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_l2_for_miss_reorder[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                x.m_l2_for_feed[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_l2_for_feed[PTI_STAT_TREQ][PTI_STAT_REMOTE] +
                    x.m_l2_for_feed[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_l2_for_feed[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                x.m_reorder_cost[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_reorder_cost[PTI_STAT_TREQ][PTI_STAT_REMOTE] +
                    x.m_reorder_cost[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_reorder_cost[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                x.m_block_cost[PTI_STAT_PREQ],
                x.m_inv_cost[PTI_STAT_TREQ][PTI_STAT_LOCAL] + x.m_inv_cost[PTI_STAT_TREQ][PTI_STAT_REMOTE],
                x.m_inv_cost[PTI_STAT_PREQ][PTI_STAT_LOCAL] + x.m_inv_cost[PTI_STAT_PREQ][PTI_STAT_REMOTE],
                x.m_req_nas[PTI_STAT_EMPTY_REQ][PTI_STAT_LOCAL],
                x.m_l2_for_emptyReq,
                x.m_inv_cost[PTI_STAT_EMPTY_REQ][PTI_STAT_LOCAL] + x.m_inv_cost[PTI_STAT_EMPTY_REQ][PTI_STAT_REMOTE],
                x.m_block_cost[PTI_STAT_EMPTY_REQ],
                x.m_req_nas[PTI_STAT_DRAMCTRL_READ_REQ][PTI_STAT_LOCAL] + x.m_req_nas[PTI_STAT_DRAMCTRL_READ_REQ][PTI_STAT_REMOTE],
                x.m_req_nas[PTI_STAT_DRAMCTRL_WRITE_REQ][PTI_STAT_LOCAL] + x.m_req_nas[PTI_STAT_DRAMCTRL_WRITE_REQ][PTI_STAT_REMOTE],
                x.m_dram_ops,
                x.m_rep_nas[PTI_STAT_DRAMCTRL_REP][PTI_STAT_LOCAL] + x.m_rep_nas[PTI_STAT_DRAMCTRL_REP][PTI_STAT_REMOTE],
                x.m_rep_nas[PTI_STAT_TREP][PTI_STAT_LOCAL] + x.m_rep_nas[PTI_STAT_TREP][PTI_STAT_REMOTE],
                x.m_rep_nas[PTI_STAT_PREP][PTI_STAT_LOCAL] + x.m_rep_nas[PTI_STAT_PREP][PTI_STAT_REMOTE],
                x.m_req_nas[PTI_STAT_RREQ][PTI_STAT_LOCAL] + x.m_req_nas[PTI_STAT_RREQ][PTI_STAT_REMOTE]);
    }
    
    out << str << endl;
    out << endl;

    out << "[P1S2PTI EXEC DUMP]" << endl;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        uint32_t id = it->first;
        std::shared_ptr<privateSharedPTIStatsPerTile> st = static_pointer_cast<privateSharedPTIStatsPerTile>(it->second);
        privateSharedPTIStatsPerMemInstr &x = st->m_total_per_mem_instr_info;
        out << "---- " << id << " counter " << endl;
        out << st->m_num_l1_read_instr << " ";
        out << st->m_num_l1_write_instr << " ";
        out << st->m_num_l1_read_hit_on_T << " ";
        out << st->m_num_l1_read_hit_on_P << " ";
        out << st->m_num_l1_read_miss_true << " ";
        out << st->m_num_l1_read_miss_on_expired_T << " ";
        out << st->m_num_l1_write_hit << " ";
        out << st->m_num_l1_write_miss_true << " ";
        out << st->m_num_l1_write_miss_on_T << " ";
        out << st->m_num_core_miss_read_instr << " ";
        out << st->m_num_core_miss_write_instr << " ";
        out << st->m_num_evict_from_l2 << " ";
        out << st->m_num_cat_action << " ";
        out << st->m_num_l1_action << " ";
        out << st->m_num_l2_action << " ";
        out << st->m_num_dram_action << " ";
        out << st->m_num_short_tReq << " ";

        for (int i = 0; i < PTI_STAT_NUM_SUB_TYPES; ++i) {
            out << st->m_num_l2_read_instr[i] << " ";
            out << st->m_num_l2_write_instr[i] << " ";
            out << st->m_num_l2_read_hit_on_valid_T[i] << " ";
            out << st->m_num_l2_read_hit_on_expired_T[i] << " ";
            out << st->m_num_l2_read_miss_true[i] << " ";
            out << st->m_num_l2_read_miss_on_P[i] << " ";
            out << st->m_num_l2_read_miss_on_valid_T[i] << " ";
            out << st->m_num_l2_read_miss_reorder[i] << " ";
            out << st->m_num_l2_write_hit_on_expired_T[i] << " ";
            out << st->m_num_l2_write_miss_true[i] << " ";
            out << st->m_num_l2_write_miss_on_valid_T[i] << " ";
            out << st->m_num_l2_write_miss_on_P[i] << " ";
            out << st->m_num_l2_write_miss_reorder[i] << " ";
            out << st->m_num_evict_from_l1[i] << " ";
            out << st->m_num_writeback_from_l1[i] << " ";
        }

        for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
            out << st->m_num_block[i] << " ";
            for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
                out << st->m_num_inv[i][j] << " ";
                out << st->m_num_reorder[i][j] << " ";
                out << st->m_req_sent[i][j] << " ";
            }
        }

        for (int i = 0; i < PTI_STAT_NUM_REP_TYPES; ++i) {
            for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
                out << st->m_rep_sent[i][j] << " ";
            }
        }

        out << endl;

        out << "---- " << id << " latency " << endl;
        out << x.m_short_latency << " ";
        out << x.m_mem_srz << " ";
        out << x.m_cat_srz << " ";
        out << x.m_cat_ops << " ";
        out << x.m_dram_ops << " ";
        out << x.m_mig << " ";
        out << x.m_l1_srz<< " ";
        out << x.m_l1_ops<< " ";
        out << x.m_l1_for_read_hit_on_T << " ";
        out << x.m_l1_for_read_hit_on_P << " ";
        out << x.m_l1_for_read_miss_on_expired_T << " ";
        out << x.m_l1_for_read_miss_true << " ";
        out << x.m_l1_for_write_hit << " ";
        out << x.m_l1_for_write_miss_true << " ";
        out << x.m_l1_for_write_miss_on_T << " ";
        out << x.m_l1_for_feed_for_read << " ";
        out << x.m_l1_for_feed_for_write << " ";
        out << x.m_l2_srz << " ";
        out << x.m_l2_ops << " ";
        out << x.m_l2_for_emptyReq << " ";

        for (int i = 0; i < PTI_STAT_NUM_SUB_TYPES; ++i) {
            out << x.m_bypass[i] << " ";
        }

        for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
            out << x.m_block_cost[i] << " ";
            for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
                out << x.m_inv_cost[i][j] << " ";
                out << x.m_reorder_cost[i][j] << " ";
                out << x.m_req_nas[i][j] << " ";
                out << x.m_rep_nas[i][j] << " ";
                out << x.m_l2_for_hit_on_valid_T[i][j] << " ";
                out << x.m_l2_for_hit_on_expired_T[i][j] << " ";
                out << x.m_l2_for_miss_true[i][j] << " ";
                out << x.m_l2_for_miss_on_valid_T[i][j] << " ";
                out << x.m_l2_for_miss_on_P[i][j] << " ";
                out << x.m_l2_for_miss_reorder[i][j] << " ";
                out << x.m_l2_for_feed[i][j] << " ";
            }
        }
        out << endl;
    }

    out << "---- Total counter " << endl;
    out << t.m_num_l1_read_instr << " ";
    out << t.m_num_l1_write_instr << " ";
    out << t.m_num_l1_read_hit_on_T << " ";
    out << t.m_num_l1_read_hit_on_P << " ";
    out << t.m_num_l1_read_miss_true << " ";
    out << t.m_num_l1_read_miss_on_expired_T << " ";
    out << t.m_num_l1_write_hit << " ";
    out << t.m_num_l1_write_miss_true << " ";
    out << t.m_num_l1_write_miss_on_T << " ";
    out << t.m_num_core_miss_read_instr << " ";
    out << t.m_num_core_miss_write_instr << " ";
    out << t.m_num_evict_from_l2 << " ";
    out << t.m_num_cat_action << " ";
    out << t.m_num_l1_action << " ";
    out << t.m_num_l2_action << " ";
    out << t.m_num_dram_action << " ";
    out << t.m_num_short_tReq << " ";

    for (int i = 0; i < PTI_STAT_NUM_SUB_TYPES; ++i) {
        out << t.m_num_l2_read_instr[i] << " ";
        out << t.m_num_l2_write_instr[i] << " ";
        out << t.m_num_l2_read_hit_on_valid_T[i] << " ";
        out << t.m_num_l2_read_hit_on_expired_T[i] << " ";
        out << t.m_num_l2_read_miss_true[i] << " ";
        out << t.m_num_l2_read_miss_on_P[i] << " ";
        out << t.m_num_l2_read_miss_on_valid_T[i] << " ";
        out << t.m_num_l2_read_miss_reorder[i] << " ";
        out << t.m_num_l2_write_hit_on_expired_T[i] << " ";
        out << t.m_num_l2_write_miss_true[i] << " ";
        out << t.m_num_l2_write_miss_on_valid_T[i] << " ";
        out << t.m_num_l2_write_miss_on_P[i] << " ";
        out << t.m_num_l2_write_miss_reorder[i] << " ";
        out << t.m_num_evict_from_l1[i] << " ";
        out << t.m_num_writeback_from_l1[i] << " ";
    }

    for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
        out << t.m_num_block[i] << " ";
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            out << t.m_num_inv[i][j] << " ";
            out << t.m_num_reorder[i][j] << " ";
            out << t.m_req_sent[i][j] << " ";
        }
    }

    for (int i = 0; i < PTI_STAT_NUM_REP_TYPES; ++i) {
        for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
            out << t.m_rep_sent[i][j] << " ";
        }
    }
    out << endl;

    {
        privateSharedPTIStatsPerMemInstr &x = t.m_total_per_mem_instr_info;
        out << "---- total latency " << endl;
        out << x.m_short_latency << " ";
        out << x.m_mem_srz << " ";
        out << x.m_cat_srz << " ";
        out << x.m_cat_ops << " ";
        out << x.m_dram_ops << " ";
        out << x.m_mig << " ";
        out << x.m_l1_srz<< " ";
        out << x.m_l1_ops<< " ";
        out << x.m_l1_for_read_hit_on_T << " ";
        out << x.m_l1_for_read_hit_on_P << " ";
        out << x.m_l1_for_read_miss_on_expired_T << " ";
        out << x.m_l1_for_read_miss_true << " ";
        out << x.m_l1_for_write_hit << " ";
        out << x.m_l1_for_write_miss_true << " ";
        out << x.m_l1_for_write_miss_on_T << " ";
        out << x.m_l1_for_feed_for_read << " ";
        out << x.m_l1_for_feed_for_write << " ";
        out << x.m_l2_srz << " ";
        out << x.m_l2_ops << " ";
        out << x.m_l2_for_emptyReq << " ";


        for (int i = 0; i < PTI_STAT_NUM_SUB_TYPES; ++i) {
            out << x.m_bypass[i] << " ";
        }

        for (int i = 0; i < PTI_STAT_NUM_REQ_TYPES; ++i) {
            out << x.m_block_cost[i] << " ";
            for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
                out << x.m_inv_cost[i][j] << " ";
                out << x.m_reorder_cost[i][j] << " ";
                out << x.m_req_nas[i][j] << " ";
                out << x.m_l2_for_hit_on_valid_T[i][j] << " ";
                out << x.m_l2_for_hit_on_expired_T[i][j] << " ";
                out << x.m_l2_for_miss_true[i][j] << " ";
                out << x.m_l2_for_miss_on_valid_T[i][j] << " ";
                out << x.m_l2_for_miss_on_P[i][j] << " ";
                out << x.m_l2_for_miss_reorder[i][j] << " ";
                out << x.m_l2_for_feed[i][j] << " ";
            }
        }

        for (int i = 0; i < PTI_STAT_NUM_REP_TYPES; ++i) {
            for (int j = 0; j < PTI_STAT_NUM_SUB_TYPES; ++j) {
                out << x.m_rep_nas[i][j] << " ";
            }
        }

        out << endl;
    }

}

