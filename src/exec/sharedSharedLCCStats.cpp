// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "sharedSharedLCCStats.hpp"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(true) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

sharedSharedLCCStatsPerMemInstr::sharedSharedLCCStatsPerMemInstr(bool is_read) :
    m_is_read(is_read),
    m_did_core_miss(false),
    m_in_migration(false),
    m_mig_depart_time(0),
    m_mem_srz(0),
    m_l1_srz(0),
    m_l1_ops(0),
    m_l1_blk_by_checkin(0),
    m_l1_blk_by_ts(0),
    m_cat_srz(0),
    m_cat_ops(0),
    m_remote_req_nas(0),
    m_remote_rep_nas(0),
    m_remote_checkin_nas(0),
    m_l2_srz(0),
    m_l2_ops(0),
    m_l2_blk_by_checkin(0),
    m_l2_blk_by_ts(0),
    m_dramctrl_req_nas(0),
    m_dramctrl_rep_nas(0),
    m_dram_ops(0),
    m_bypass(0),
    m_mig(0),
    m_local_l1_cost_for_hit(0),
    m_local_l1_cost_for_miss(0),
    m_local_l1_cost_for_update(0),
    m_local_l1_blk_by_checkin(0),
    m_local_l1_blk_by_ts(0),
    m_local_bypass(0),
    m_remote_l1_cost_for_hit(0),
    m_remote_l1_cost_for_miss(0),
    m_remote_l1_cost_for_update(0),
    m_remote_l1_blk_by_checkin(0),
    m_remote_l1_blk_by_ts(0),
    m_remote_bypass(0),
    m_local_l2_cost_for_hit(0),
    m_local_l2_cost_for_miss(0),
    m_local_l2_cost_for_update(0),
    m_local_l2_cost_for_writeback(0),
    m_local_l2_blk_by_checkin(0),
    m_local_l2_blk_by_ts(0),
    m_remote_l2_cost_for_hit(0),
    m_remote_l2_cost_for_miss(0),
    m_remote_l2_cost_for_update(0),
    m_remote_l2_cost_for_writeback(0),
    m_remote_l2_blk_by_checkin(0),
    m_remote_l2_blk_by_ts(0)
{}

sharedSharedLCCStatsPerMemInstr::~sharedSharedLCCStatsPerMemInstr() {}

void sharedSharedLCCStatsPerMemInstr::add(const sharedSharedLCCStatsPerMemInstr& other) {
    m_mem_srz += other.m_mem_srz;
    m_l1_srz += other.m_l1_srz;
    m_l1_ops += other.m_l1_ops;
    m_l1_blk_by_checkin += other.m_l1_blk_by_checkin;
    m_l1_blk_by_ts += other.m_l1_blk_by_ts;
    m_cat_srz += other.m_cat_srz;
    m_cat_ops += other.m_cat_ops;
    m_remote_req_nas += other.m_remote_req_nas;
    m_remote_rep_nas += other.m_remote_rep_nas;
    m_remote_checkin_nas += other.m_remote_checkin_nas;
    m_l2_srz += other.m_l2_srz;
    m_l2_ops += other.m_l2_ops;
    m_l2_blk_by_checkin += other.m_l2_blk_by_checkin;
    m_l2_blk_by_ts += other.m_l2_blk_by_ts;
    m_dramctrl_req_nas += other.m_dramctrl_req_nas;
    m_dramctrl_rep_nas += other.m_dramctrl_rep_nas;
    m_dram_ops += other.m_dram_ops;
    m_bypass += other.m_bypass;
    m_mig += other.m_mig;
    m_local_l1_cost_for_hit += other.m_local_l1_cost_for_hit;
    m_local_l1_cost_for_miss += other.m_local_l1_cost_for_miss;
    m_local_l1_cost_for_update += other.m_local_l1_cost_for_update;
    m_local_l1_blk_by_checkin += other.m_local_l1_blk_by_checkin;
    m_local_l1_blk_by_ts += other.m_local_l1_blk_by_ts;
    m_local_bypass += other.m_local_bypass;
    m_remote_l1_cost_for_hit += other.m_remote_l1_cost_for_hit;
    m_remote_l1_cost_for_miss += other.m_remote_l1_cost_for_miss;
    m_remote_l1_cost_for_update += other.m_remote_l1_cost_for_update;
    m_remote_l1_blk_by_checkin += other.m_remote_l1_blk_by_checkin;
    m_remote_l1_blk_by_ts += other.m_remote_l1_blk_by_ts;
    m_remote_bypass += other.m_remote_bypass;
    m_local_l2_cost_for_hit += other.m_local_l2_cost_for_hit;
    m_local_l2_cost_for_miss += other.m_local_l2_cost_for_miss;
    m_local_l2_cost_for_update += other.m_local_l2_cost_for_update;
    m_local_l2_cost_for_writeback += other.m_local_l2_cost_for_writeback;
    m_local_l2_blk_by_checkin += other.m_local_l2_blk_by_checkin;
    m_local_l2_blk_by_ts += other.m_local_l2_blk_by_ts;
    m_remote_l2_cost_for_hit += other.m_remote_l2_cost_for_hit;
    m_remote_l2_cost_for_miss += other.m_remote_l2_cost_for_miss;
    m_remote_l2_cost_for_update += other.m_remote_l2_cost_for_update;
    m_remote_l2_cost_for_writeback += other.m_remote_l2_cost_for_writeback;
    m_remote_l2_blk_by_checkin += other.m_remote_l2_blk_by_checkin;
    m_remote_l2_blk_by_ts += other.m_remote_l2_blk_by_ts;
}

uint64_t sharedSharedLCCStatsPerMemInstr::total_cost() { 
    return m_mem_srz + m_cat_srz + m_cat_ops + m_remote_req_nas + m_remote_rep_nas + m_remote_checkin_nas 
           + m_l1_srz + m_l1_ops + m_l1_blk_by_checkin + m_l1_blk_by_ts
           + m_l2_srz + m_l2_ops + m_l2_blk_by_checkin + m_l2_blk_by_ts 
           + m_dramctrl_req_nas + m_dramctrl_rep_nas + m_dram_ops + m_bypass + m_mig;
}

bool sharedSharedLCCStatsPerMemInstr::add_new_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        return false;
    }
    shared_ptr<sharedSharedLCCStatsPerMemInstr> new_tentative_set(new sharedSharedLCCStatsPerMemInstr(m_is_read));
    m_tentative_data[index] = new_tentative_set;
    return true;
}

shared_ptr<sharedSharedLCCStatsPerMemInstr> sharedSharedLCCStatsPerMemInstr::get_tentative_data(int index) {
    if (!m_tentative_data.count(index)) {
        add_new_tentative_data(index);
    }
    return m_tentative_data[index];
}

int sharedSharedLCCStatsPerMemInstr::get_max_tentative_data_index() {
    mh_assert(m_tentative_data.size());
    uint64_t value = 0;
    map<int, shared_ptr<sharedSharedLCCStatsPerMemInstr> >::iterator it;
    int max = 0;
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
        shared_ptr<sharedSharedLCCStatsPerMemInstr> cur = it->second;
        if (cur->total_cost() > value) {
            value = cur->total_cost();
            max = it->first;
        }
    }

    return max;
}

void sharedSharedLCCStatsPerMemInstr::discard_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        m_tentative_data.erase(index);
    } 
}

void sharedSharedLCCStatsPerMemInstr::commit_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        add(*m_tentative_data[index]);
    }
    m_tentative_data.clear();
}

void sharedSharedLCCStatsPerMemInstr::commit_max_tentative_data() {
    if (m_tentative_data.size() == 0) {
        return;
    }

    add(*m_tentative_data[get_max_tentative_data_index()]);
    m_tentative_data.clear();
}

void sharedSharedLCCStatsPerMemInstr::commit_min_tentative_data() {
    uint64_t value = UINT64_MAX;
    map<int, shared_ptr<sharedSharedLCCStatsPerMemInstr> >::iterator it;
    shared_ptr<sharedSharedLCCStatsPerMemInstr> min = shared_ptr<sharedSharedLCCStatsPerMemInstr>();
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
        shared_ptr<sharedSharedLCCStatsPerMemInstr> cur = it->second;
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

void sharedSharedLCCStatsPerMemInstr::migration_finished(const uint64_t arrival_time, bool write_stats) {
    if (m_in_migration) {
        if (write_stats) {
            m_mig += arrival_time - m_mig_depart_time;
        }
        m_in_migration = false;
    }
}

sharedSharedLCCStatsPerTile::sharedSharedLCCStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t),
    m_num_l1_read_instr(0), 
    m_num_l1_write_instr(0), 
    m_num_hits_for_read_instr_at_home_l1(0), 
    m_num_hits_for_write_instr_at_home_l1(0), 
    m_num_true_misses_for_read_instr_at_home_l1(0), 
    m_num_true_misses_for_write_instr_at_home_l1(0), 
    m_num_ts_blocked_miss_for_write_instr_at_home_l1(0), 
    m_num_checkin_blocked_miss_for_read_instr_at_home_l1(0), 
    m_num_checkin_blocked_miss_for_write_instr_at_home_l1(0), 
    m_num_hits_for_read_instr_at_away_l1(0), 
    m_num_hits_for_write_instr_at_away_l1(0), 
    m_num_true_misses_for_read_instr_at_away_l1(0), 
    m_num_true_misses_for_write_instr_at_away_l1(0), 
    m_num_ts_expired_miss_for_read_instr_at_away_l1(0), 
    m_num_permission_miss_ts_expired_for_write_instr_at_away_l1(0), 
    m_num_permission_miss_ts_unexpired_for_write_instr_at_away_l1(0), 
    m_num_hits_for_read_instr_at_local_l1(0), 
    m_num_hits_for_write_instr_at_local_l1(0), 
    m_num_true_misses_for_read_instr_at_local_l1(0), 
    m_num_true_misses_for_write_instr_at_local_l1(0), 
    m_num_ts_blocked_miss_for_write_instr_at_local_l1(0), 
    m_num_checkin_blocked_miss_for_read_instr_at_local_l1(0), 
    m_num_checkin_blocked_miss_for_write_instr_at_local_l1(0), 
    m_num_ts_expired_miss_for_read_instr_at_local_l1(0), 
    m_num_permission_miss_ts_expired_for_write_instr_at_local_l1(0), 
    m_num_permission_miss_ts_unexpired_for_write_instr_at_local_l1(0), 
    m_num_hits_for_read_instr_at_remote_l1(0), 
    m_num_hits_for_write_instr_at_remote_l1(0), 
    m_num_true_misses_for_read_instr_at_remote_l1(0), 
    m_num_true_misses_for_write_instr_at_remote_l1(0), 
    m_num_ts_blocked_miss_for_write_instr_at_remote_l1(0), 
    m_num_checkin_blocked_miss_for_read_instr_at_remote_l1(0), 
    m_num_checkin_blocked_miss_for_write_instr_at_remote_l1(0), 
    m_num_hits_for_local_l2_read_instr(0),
    m_num_hits_for_local_l2_write_instr(0),
    m_num_true_misses_for_read_instr_at_local_l2(0),
    m_num_true_misses_for_write_instr_at_local_l2(0),
    m_num_ts_blocked_miss_for_write_instr_at_local_l2(0),
    m_num_checkin_blocked_miss_for_read_instr_at_local_l2(0),
    m_num_checkin_blocked_miss_for_write_instr_at_local_l2(0),
    m_num_hits_for_remote_l2_read_instr(0),
    m_num_hits_for_remote_l2_write_instr(0),
    m_num_true_misses_for_read_instr_at_remote_l2(0),
    m_num_true_misses_for_write_instr_at_remote_l2(0),
    m_num_ts_blocked_miss_for_write_instr_at_remote_l2(0),
    m_num_checkin_blocked_miss_for_read_instr_at_remote_l2(0),
    m_num_checkin_blocked_miss_for_write_instr_at_remote_l2(0),
    m_num_l2_read_instr(0), 
    m_num_l2_write_instr(0), 
    m_num_hits_for_l2_read_instr(0), 
    m_num_hits_for_l2_write_instr(0), 
    m_num_true_misses_for_read_instr_at_l2(0), 
    m_num_true_misses_for_write_instr_at_l2(0), 
    m_num_ts_blocked_miss_for_write_instr_at_l2(0), 
    m_num_checkin_blocked_miss_for_read_instr_at_l2(0), 
    m_num_checkin_blocked_miss_for_write_instr_at_l2(0), 
    m_num_bypass_for_read_instr(0), 
    m_num_bypass_for_local_read_instr(0), 
    m_num_bypass_for_remote_read_instr(0), 
    m_num_core_hit_read_instr(0),
    m_num_core_hit_write_instr(0),
    m_num_core_miss_read_instr(0),
    m_num_core_miss_write_instr(0),
    m_remote_checkin_uncacheable_sent(0), 
    m_remote_checkin_expired_sent(0), 
    m_remote_checkin_evicted_sent(0), 
    m_checkin_hit_at_l1(0), 
    m_checkin_hit_at_l2(0), 
    m_num_evict_at_l1(0), 
    m_num_evict_at_l2(0), 
    m_num_writeback_at_l1(0), 
    m_num_writeback_at_l2(0), 
    m_num_cat_action(0), 
    m_num_l1_action(0), 
    m_num_l2_action(0), 
    m_num_dram_action(0),
    m_total_per_mem_instr_info(false/*don't care*/)
{}

sharedSharedLCCStatsPerTile::~sharedSharedLCCStatsPerTile() {}

void sharedSharedLCCStatsPerTile::add(const sharedSharedLCCStatsPerTile& other) {
    m_num_l1_read_instr += other.m_num_l1_read_instr; 
    m_num_l1_write_instr += other.m_num_l1_write_instr; 
    m_num_hits_for_read_instr_at_home_l1 += other.m_num_hits_for_read_instr_at_home_l1; 
    m_num_hits_for_write_instr_at_home_l1 += other.m_num_hits_for_write_instr_at_home_l1; 
    m_num_true_misses_for_read_instr_at_home_l1 += other.m_num_true_misses_for_read_instr_at_home_l1; 
    m_num_true_misses_for_write_instr_at_home_l1 += other.m_num_true_misses_for_write_instr_at_home_l1; 
    m_num_ts_blocked_miss_for_write_instr_at_home_l1 += other.m_num_ts_blocked_miss_for_write_instr_at_home_l1; 
    m_num_checkin_blocked_miss_for_read_instr_at_home_l1 += other.m_num_checkin_blocked_miss_for_read_instr_at_home_l1; 
    m_num_checkin_blocked_miss_for_write_instr_at_home_l1 += other.m_num_checkin_blocked_miss_for_write_instr_at_home_l1; 
    m_num_hits_for_read_instr_at_away_l1 += other.m_num_hits_for_read_instr_at_away_l1; 
    m_num_hits_for_write_instr_at_away_l1 += other.m_num_hits_for_write_instr_at_away_l1; 
    m_num_true_misses_for_read_instr_at_away_l1 += other.m_num_true_misses_for_read_instr_at_away_l1; 
    m_num_true_misses_for_write_instr_at_away_l1 += other.m_num_true_misses_for_write_instr_at_away_l1; 
    m_num_ts_expired_miss_for_read_instr_at_away_l1 += other.m_num_ts_expired_miss_for_read_instr_at_away_l1; 
    m_num_permission_miss_ts_expired_for_write_instr_at_away_l1 += other.m_num_permission_miss_ts_expired_for_write_instr_at_away_l1; 
    m_num_permission_miss_ts_unexpired_for_write_instr_at_away_l1 += other.m_num_permission_miss_ts_unexpired_for_write_instr_at_away_l1; 
    m_num_hits_for_read_instr_at_local_l1 += other.m_num_hits_for_read_instr_at_local_l1; 
    m_num_hits_for_write_instr_at_local_l1 += other.m_num_hits_for_write_instr_at_local_l1; 
    m_num_true_misses_for_read_instr_at_local_l1 += other.m_num_true_misses_for_read_instr_at_local_l1; 
    m_num_true_misses_for_write_instr_at_local_l1 += other.m_num_true_misses_for_write_instr_at_local_l1; 
    m_num_ts_blocked_miss_for_write_instr_at_local_l1 += other.m_num_ts_blocked_miss_for_write_instr_at_local_l1; 
    m_num_checkin_blocked_miss_for_read_instr_at_local_l1 += other.m_num_checkin_blocked_miss_for_read_instr_at_local_l1; 
    m_num_checkin_blocked_miss_for_write_instr_at_local_l1 += other.m_num_checkin_blocked_miss_for_write_instr_at_local_l1; 
    m_num_ts_expired_miss_for_read_instr_at_local_l1 += other.m_num_ts_expired_miss_for_read_instr_at_local_l1; 
    m_num_permission_miss_ts_expired_for_write_instr_at_local_l1 += other.m_num_permission_miss_ts_expired_for_write_instr_at_local_l1; 
    m_num_permission_miss_ts_unexpired_for_write_instr_at_local_l1 += other.m_num_permission_miss_ts_unexpired_for_write_instr_at_local_l1; 
    m_num_hits_for_read_instr_at_remote_l1 += other.m_num_hits_for_read_instr_at_remote_l1; 
    m_num_hits_for_write_instr_at_remote_l1 += other.m_num_hits_for_write_instr_at_remote_l1; 
    m_num_true_misses_for_read_instr_at_remote_l1 += other.m_num_true_misses_for_read_instr_at_remote_l1; 
    m_num_true_misses_for_write_instr_at_remote_l1 += other.m_num_true_misses_for_write_instr_at_remote_l1; 
    m_num_ts_blocked_miss_for_write_instr_at_remote_l1 += other.m_num_ts_blocked_miss_for_write_instr_at_remote_l1; 
    m_num_checkin_blocked_miss_for_read_instr_at_remote_l1 += other.m_num_checkin_blocked_miss_for_read_instr_at_remote_l1; 
    m_num_checkin_blocked_miss_for_write_instr_at_remote_l1 += other.m_num_checkin_blocked_miss_for_write_instr_at_remote_l1; 
    m_num_hits_for_local_l2_read_instr += other.m_num_hits_for_local_l2_read_instr;
    m_num_hits_for_local_l2_write_instr += other.m_num_hits_for_local_l2_write_instr;
    m_num_true_misses_for_read_instr_at_local_l2 += other.m_num_true_misses_for_read_instr_at_local_l2;
    m_num_true_misses_for_write_instr_at_local_l2 += other.m_num_true_misses_for_write_instr_at_local_l2;
    m_num_ts_blocked_miss_for_write_instr_at_local_l2 += other.m_num_ts_blocked_miss_for_write_instr_at_local_l2;
    m_num_checkin_blocked_miss_for_read_instr_at_local_l2 += other.m_num_checkin_blocked_miss_for_read_instr_at_local_l2;
    m_num_checkin_blocked_miss_for_write_instr_at_local_l2 += other.m_num_checkin_blocked_miss_for_write_instr_at_local_l2;
    m_num_hits_for_remote_l2_read_instr += other.m_num_hits_for_remote_l2_read_instr;
    m_num_hits_for_remote_l2_write_instr += other.m_num_hits_for_remote_l2_write_instr;
    m_num_true_misses_for_read_instr_at_remote_l2 += other.m_num_true_misses_for_read_instr_at_remote_l2;
    m_num_true_misses_for_write_instr_at_remote_l2 += other.m_num_true_misses_for_write_instr_at_remote_l2;
    m_num_ts_blocked_miss_for_write_instr_at_remote_l2 += other.m_num_ts_blocked_miss_for_write_instr_at_remote_l2;
    m_num_checkin_blocked_miss_for_read_instr_at_remote_l2 += other.m_num_checkin_blocked_miss_for_read_instr_at_remote_l2;
    m_num_checkin_blocked_miss_for_write_instr_at_remote_l2 += other.m_num_checkin_blocked_miss_for_write_instr_at_remote_l2;
    m_num_l2_read_instr += other.m_num_l2_read_instr; 
    m_num_l2_write_instr += other.m_num_l2_write_instr; 
    m_num_hits_for_l2_read_instr += other.m_num_hits_for_l2_read_instr; 
    m_num_hits_for_l2_write_instr += other.m_num_hits_for_l2_write_instr; 
    m_num_true_misses_for_read_instr_at_l2 += other.m_num_true_misses_for_read_instr_at_l2; 
    m_num_true_misses_for_write_instr_at_l2 += other.m_num_true_misses_for_write_instr_at_l2; 
    m_num_ts_blocked_miss_for_write_instr_at_l2 += other.m_num_ts_blocked_miss_for_write_instr_at_l2; 
    m_num_checkin_blocked_miss_for_read_instr_at_l2 += other.m_num_checkin_blocked_miss_for_read_instr_at_l2; 
    m_num_checkin_blocked_miss_for_write_instr_at_l2 += other.m_num_checkin_blocked_miss_for_write_instr_at_l2; 
    m_num_bypass_for_read_instr += other.m_num_bypass_for_read_instr; 
    m_num_bypass_for_local_read_instr += other.m_num_bypass_for_local_read_instr; 
    m_num_bypass_for_remote_read_instr += other.m_num_bypass_for_remote_read_instr; 
    m_num_core_hit_read_instr += other.m_num_core_hit_read_instr; 
    m_num_core_hit_write_instr += other.m_num_core_hit_write_instr; 
    m_num_core_miss_read_instr += other.m_num_core_miss_read_instr; 
    m_num_core_miss_write_instr += other.m_num_core_miss_write_instr; 
    m_remote_checkin_uncacheable_sent += other.m_remote_checkin_uncacheable_sent; 
    m_remote_checkin_expired_sent += other.m_remote_checkin_expired_sent; 
    m_remote_checkin_evicted_sent += other.m_remote_checkin_evicted_sent; 
    m_checkin_hit_at_l1 += other.m_checkin_hit_at_l1; 
    m_checkin_hit_at_l2 += other.m_checkin_hit_at_l2; 
    m_num_evict_at_l1 += other.m_num_evict_at_l1; 
    m_num_evict_at_l2 += other.m_num_evict_at_l2; 
    m_num_writeback_at_l1 += other.m_num_writeback_at_l1; 
    m_num_writeback_at_l2 += other.m_num_writeback_at_l2; 
    m_num_cat_action += other.m_num_cat_action; 
    m_num_l1_action += other.m_num_l1_action; 
    m_num_l2_action += other.m_num_l2_action; 
    m_num_dram_action += other.m_num_dram_action;

    m_total_per_mem_instr_info.add(other.m_total_per_mem_instr_info);
}

void sharedSharedLCCStatsPerTile::commit_per_mem_instr_stats(const sharedSharedLCCStatsPerMemInstr& data) {

    if (data.m_did_core_miss) {
        if (data.m_is_read) {
            ++m_num_core_miss_read_instr;
        } else {
            ++m_num_core_miss_write_instr;
        }
    } else {
        if (data.m_is_read) {
            ++m_num_core_hit_read_instr;
        } else {
            ++m_num_core_hit_write_instr;
        }
    }

    m_total_per_mem_instr_info.add(data);

}

sharedSharedLCCStats::sharedSharedLCCStats(const uint64_t &t) : memStats(t) {}

sharedSharedLCCStats::~sharedSharedLCCStats() {}

void sharedSharedLCCStats::print_stats(ostream &out) {

    char str[4096];
    sharedSharedLCCStatsPerTile total_tile_info(0, system_time);

    memStats::print_stats(out);

    /* add sharedSharedLCC-specific statistics */

    out << endl;
    
    out << "Shared-L1 Shared-L2 LCC Statistics" << endl;
    out << "-----------------------------------" << endl;
    out << endl;

    perTileStats_t::iterator it;
    uint32_t num_tiles = 0;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        ++num_tiles;
        uint32_t id = it->first;
        shared_ptr<sharedSharedLCCStatsPerTile> st = static_pointer_cast<sharedSharedLCCStatsPerTile>(it->second);

        sprintf(str, "[S1S2LCC:Core %d ] "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld",
                id,
                st->m_num_l1_read_instr, 
                st->m_num_l1_write_instr, 
                st->m_num_hits_for_read_instr_at_home_l1, 
                st->m_num_hits_for_write_instr_at_home_l1, 
                st->m_num_true_misses_for_read_instr_at_home_l1, 
                st->m_num_true_misses_for_write_instr_at_home_l1, 
                st->m_num_ts_blocked_miss_for_write_instr_at_home_l1, 
                st->m_num_checkin_blocked_miss_for_read_instr_at_home_l1, 
                st->m_num_checkin_blocked_miss_for_write_instr_at_home_l1, 
                st->m_num_bypass_for_read_instr, 
                st->m_num_hits_for_read_instr_at_away_l1, 
                st->m_num_hits_for_write_instr_at_away_l1, 
                st->m_num_true_misses_for_read_instr_at_away_l1, 
                st->m_num_true_misses_for_write_instr_at_away_l1, 
                st->m_num_ts_expired_miss_for_read_instr_at_away_l1, 
                st->m_num_permission_miss_ts_expired_for_write_instr_at_away_l1, 
                st->m_num_permission_miss_ts_unexpired_for_write_instr_at_away_l1, 
                st->m_num_l2_read_instr, 
                st->m_num_l2_write_instr, 
                st->m_num_hits_for_l2_read_instr, 
                st->m_num_hits_for_l2_write_instr, 
                st->m_num_true_misses_for_read_instr_at_l2, 
                st->m_num_true_misses_for_write_instr_at_l2, 
                st->m_num_ts_blocked_miss_for_write_instr_at_l2, 
                st->m_num_checkin_blocked_miss_for_read_instr_at_l2, 
                st->m_num_checkin_blocked_miss_for_write_instr_at_l2, 
                st->m_num_core_hit_read_instr, 
                st->m_num_core_hit_write_instr, 
                st->m_num_core_miss_read_instr, 
                st->m_num_core_miss_write_instr, 
                st->m_remote_checkin_uncacheable_sent, 
                st->m_remote_checkin_expired_sent, 
                st->m_remote_checkin_evicted_sent, 
                st->m_checkin_hit_at_l1, 
                st->m_checkin_hit_at_l2, 
                st->m_num_evict_at_l1, 
                st->m_num_evict_at_l2, 
                st->m_num_writeback_at_l1, 
                st->m_num_writeback_at_l2, 
                st->m_num_cat_action, 
                st->m_num_l1_action, 
                st->m_num_l2_action, 
                st->m_num_dram_action,
                st->m_num_hits_for_read_instr_at_local_l1, 
                st->m_num_hits_for_write_instr_at_local_l1, 
                st->m_num_true_misses_for_read_instr_at_local_l1, 
                st->m_num_true_misses_for_write_instr_at_local_l1, 
                st->m_num_ts_blocked_miss_for_write_instr_at_local_l1, 
                st->m_num_checkin_blocked_miss_for_read_instr_at_local_l1, 
                st->m_num_checkin_blocked_miss_for_write_instr_at_local_l1, 
                st->m_num_ts_expired_miss_for_read_instr_at_local_l1, 
                st->m_num_permission_miss_ts_expired_for_write_instr_at_local_l1, 
                st->m_num_permission_miss_ts_unexpired_for_write_instr_at_local_l1, 
                st->m_num_hits_for_read_instr_at_remote_l1, 
                st->m_num_hits_for_write_instr_at_remote_l1, 
                st->m_num_true_misses_for_read_instr_at_remote_l1, 
                st->m_num_true_misses_for_write_instr_at_remote_l1, 
                st->m_num_ts_blocked_miss_for_write_instr_at_remote_l1, 
                st->m_num_checkin_blocked_miss_for_read_instr_at_remote_l1, 
                st->m_num_checkin_blocked_miss_for_write_instr_at_remote_l1,
                st->m_num_hits_for_local_l2_read_instr,
                st->m_num_hits_for_local_l2_write_instr,
                st->m_num_true_misses_for_read_instr_at_local_l2,
                st->m_num_true_misses_for_write_instr_at_local_l2,
                st->m_num_ts_blocked_miss_for_write_instr_at_local_l2,
                st->m_num_checkin_blocked_miss_for_read_instr_at_local_l2,
                st->m_num_checkin_blocked_miss_for_write_instr_at_local_l2,
                st->m_num_hits_for_remote_l2_read_instr,
                st->m_num_hits_for_remote_l2_write_instr,
                st->m_num_true_misses_for_read_instr_at_remote_l2,
                st->m_num_true_misses_for_write_instr_at_remote_l2,
                st->m_num_ts_blocked_miss_for_write_instr_at_remote_l2,
                st->m_num_checkin_blocked_miss_for_read_instr_at_remote_l2,
                st->m_num_checkin_blocked_miss_for_write_instr_at_remote_l2);

        out << str << endl;

        total_tile_info.add(*st);

    }

    out << endl;

    sprintf(str, "[S1S2LCC:Summary A ] for each unique [instruction, core, cache level] pair\n"
                 "    -- L1 -- \n"
                 "    total-L1-read-instr-at-each-core %ld\n"
                 "    total-L1-write-instr-at-each-core %ld\n"
                 "    -- L1 home data --\n"
                 "    total-L1-read-instr-home-hits-at-each-core %ld\n"
                 "    total-L1-write-instr-home-hits-at-each-core %ld\n"
                 "    total-L1-read-instr-home-true-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-home-true-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-home-timestamp-blocked-misses-at-each-core %ld\n"
                 "    total-L1-read-instr-home-checkin-blocked-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-home-checkin-blocked-misses-at-each-core %ld\n"
                 "    -- L1 away data --\n"
                 "    total-L1-read-instr-away-hits-at-each-core %ld\n"
                 "    total-L1-write-instr-away-hits-at-each-core %ld\n"
                 "    total-L1-read-instr-away-true-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-away-true-misses-at-each-core %ld\n"
                 "    total-L1-read-instr-away-timestamp-expired-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-away-permission-misses-expired-at-each-core %ld\n"
                 "    total-L1-write-instr-away-permission-misses-unexpired-at-each-core %ld\n"
                 "    -- L2 --\n"
                 "    total-L2-read-instr-at-each-core %ld\n"
                 "    total-L2-write-instr-at-each-core %ld\n"
                 "    total-L2-read-instr-hits-at-each-core %ld\n"
                 "    total-L2-write-instr-hits-at-each-core %ld\n"
                 "    total-L2-read-instr-true-misses-at-each-core %ld\n"
                 "    total-L2-write-instr-true-misses-at-each-core %ld\n"
                 "    total-L2-write-instr-timestamp-blocked-misses-at-each-core %ld\n"
                 "    total-L2-read-instr-checkin-blocked-misses-at-each-core %ld\n"
                 "    total-L2-write-instr-checkin-blocked-misses-at-each-core %ld\n"
                 "[S1S2LCC:Summary B ] for each instruction\n"
                 "    total-core-hit-read-instr %ld\n"
                 "    total-core-hit-write-instr %ld\n"
                 "    total-core-missed-read-instr %ld\n"
                 "    total-core-missed-write-instr %ld\n"
                 "[S1S2LCC:Summary C ] at each core\n"
                 "    total-remote-checkin-sent-as-expired-on-reception %ld\n"
                 "    total-remote-checkin-sent-as-expired-and-invalidated %ld\n"
                 "    total-remote-checkin-sent-as-evicted %ld\n"
                 "    total-checkin-hit-at-L1 %ld\n"
                 "    total-checkin-hit-at-L2 %ld\n"
                 "    total-core-eviction-L1 %ld\n"
                 "    total-core-eviction-L2 %ld\n"
                 "    total-core-writeback-L1 %ld\n"
                 "    total-core-writeback-L2 %ld\n"
                 "    total-CAT-actions-at-each-core %ld\n"
                 "    total-L1-actions-at-each-core %ld\n"
                 "    total-L2-actions-at-each-core %ld\n"
                 "    total-offchip-DRAM-actions-at-each-core %ld\n"
                 "[S1S2LCC:Summary D ] Hits and misses at L1 \n"
                 "    -- Local L1 --\n"
                 "    total-L1-read-instr-bypass-at-local-L1 %ld\n"
                 "    total-L1-read-instr-hits-at-local-L1 %ld\n"
                 "    total-L1-write-instr-hits-at-local-L1 %ld\n"
                 "    total-L1-read-instr-true-misses-at-local-L1 %ld\n"
                 "    total-L1-write-instr-true-misses-at-local-L1 %ld\n"
                 "    total-L1-write-instr-timestamp-blocked-misses-at-local-L1 %ld\n"
                 "    total-L1-read-instr-checkin-blocked-misses-at-local-L1 %ld\n"
                 "    total-L1-write-instr-checkin-blocked-misses-at-local-L1 %ld\n"
                 "    total-L1-read-instr-timestamp-expired-misses-at-local-L1 %ld\n"
                 "    total-L1-write-instr-permission-misses-expired-at-local-L1 %ld\n"
                 "    total-L1-write-instr-permission-misses-unexpired-at-local-L1 %ld\n"
                 "    -- Remote L1 --\n"
                 "    total-L1-read-instr-bypass-at-remote-L1 %ld\n"
                 "    total-L1-read-instr-hits-at-remote-L1 %ld\n"
                 "    total-L1-write-instr-hits-at-remote-L1 %ld\n"
                 "    total-L1-read-instr-true-misses-at-remote-L1 %ld\n"
                 "    total-L1-write-instr-true-misses-at-remote-L1 %ld\n"
                 "    total-L1-write-instr-timestamp-blocked-misses-at-remote-L1 %ld\n"
                 "    total-L1-read-instr-checkin-blocked-misses-at-remote-L1 %ld\n"
                 "    total-L1-write-instr-checkin-blocked-misses-at-remote-L1 %ld\n"
                 "    -- Local L2 --\n"
                 "    total-L2-read-instr-hits-at-local-L2 %ld\n"
                 "    total-L2-write-instr-hits-at-local-L2 %ld\n"
                 "    total-L2-read-instr-true-misses-at-local-L2 %ld\n"
                 "    total-L2-write-instr-true-misses-at-local-L2 %ld\n"
                 "    total-L2-write-instr-timestamp-blocked-misses-at-local-L2 %ld\n"
                 "    total-L2-read-instr-checkin-blocked-misses-at-local-L2 %ld\n"
                 "    total-L2-write-instr-checkin-blocked-misses-at-local-L2 %ld\n"
                 "    -- Remote L2 --\n"
                 "    total-L2-read-instr-hits-at-remote-L2 %ld\n"
                 "    total-L2-write-instr-hits-at-remote-L2 %ld\n"
                 "    total-L2-read-instr-true-misses-at-remote-L2 %ld\n"
                 "    total-L2-write-instr-true-misses-at-remote-L2 %ld\n"
                 "    total-L2-write-instr-timestamp-blocked-misses-at-remote-L2 %ld\n"
                 "    total-L2-read-instr-checkin-blocked-misses-at-remote-L2 %ld\n"
                 "    total-L2-write-instr-checkin-blocked-misses-at-remote-L2 %ld\n",
                total_tile_info.m_num_l1_read_instr, 
                total_tile_info.m_num_l1_write_instr, 
                total_tile_info.m_num_hits_for_read_instr_at_home_l1, 
                total_tile_info.m_num_hits_for_write_instr_at_home_l1, 
                total_tile_info.m_num_true_misses_for_read_instr_at_home_l1, 
                total_tile_info.m_num_true_misses_for_write_instr_at_home_l1, 
                total_tile_info.m_num_ts_blocked_miss_for_write_instr_at_home_l1, 
                total_tile_info.m_num_checkin_blocked_miss_for_read_instr_at_home_l1, 
                total_tile_info.m_num_checkin_blocked_miss_for_write_instr_at_home_l1, 
                total_tile_info.m_num_hits_for_read_instr_at_away_l1, 
                total_tile_info.m_num_hits_for_write_instr_at_away_l1, 
                total_tile_info.m_num_true_misses_for_read_instr_at_away_l1, 
                total_tile_info.m_num_true_misses_for_write_instr_at_away_l1, 
                total_tile_info.m_num_ts_expired_miss_for_read_instr_at_away_l1, 
                total_tile_info.m_num_permission_miss_ts_expired_for_write_instr_at_away_l1, 
                total_tile_info.m_num_permission_miss_ts_unexpired_for_write_instr_at_away_l1, 
                total_tile_info.m_num_l2_read_instr, 
                total_tile_info.m_num_l2_write_instr, 
                total_tile_info.m_num_hits_for_l2_read_instr, 
                total_tile_info.m_num_hits_for_l2_write_instr, 
                total_tile_info.m_num_true_misses_for_read_instr_at_l2, 
                total_tile_info.m_num_true_misses_for_write_instr_at_l2, 
                total_tile_info.m_num_ts_blocked_miss_for_write_instr_at_l2, 
                total_tile_info.m_num_checkin_blocked_miss_for_read_instr_at_l2, 
                total_tile_info.m_num_checkin_blocked_miss_for_write_instr_at_l2, 
                total_tile_info.m_num_core_hit_read_instr, 
                total_tile_info.m_num_core_hit_write_instr, 
                total_tile_info.m_num_core_miss_read_instr, 
                total_tile_info.m_num_core_miss_write_instr, 
                total_tile_info.m_remote_checkin_uncacheable_sent, 
                total_tile_info.m_remote_checkin_expired_sent, 
                total_tile_info.m_remote_checkin_evicted_sent, 
                total_tile_info.m_checkin_hit_at_l1, 
                total_tile_info.m_checkin_hit_at_l2, 
                total_tile_info.m_num_evict_at_l1, 
                total_tile_info.m_num_evict_at_l2, 
                total_tile_info.m_num_writeback_at_l1, 
                total_tile_info.m_num_writeback_at_l2, 
                total_tile_info.m_num_cat_action, 
                total_tile_info.m_num_l1_action, 
                total_tile_info.m_num_l2_action, 
                total_tile_info.m_num_dram_action,
                total_tile_info.m_num_bypass_for_local_read_instr, 
                total_tile_info.m_num_hits_for_read_instr_at_local_l1, 
                total_tile_info.m_num_hits_for_write_instr_at_local_l1, 
                total_tile_info.m_num_true_misses_for_read_instr_at_local_l1, 
                total_tile_info.m_num_true_misses_for_write_instr_at_local_l1, 
                total_tile_info.m_num_ts_blocked_miss_for_write_instr_at_local_l1, 
                total_tile_info.m_num_checkin_blocked_miss_for_read_instr_at_local_l1, 
                total_tile_info.m_num_checkin_blocked_miss_for_write_instr_at_local_l1, 
                total_tile_info.m_num_ts_expired_miss_for_read_instr_at_local_l1, 
                total_tile_info.m_num_permission_miss_ts_expired_for_write_instr_at_local_l1, 
                total_tile_info.m_num_permission_miss_ts_unexpired_for_write_instr_at_local_l1, 
                total_tile_info.m_num_bypass_for_remote_read_instr, 
                total_tile_info.m_num_hits_for_read_instr_at_remote_l1, 
                total_tile_info.m_num_hits_for_write_instr_at_remote_l1, 
                total_tile_info.m_num_true_misses_for_read_instr_at_remote_l1, 
                total_tile_info.m_num_true_misses_for_write_instr_at_remote_l1, 
                total_tile_info.m_num_ts_blocked_miss_for_write_instr_at_remote_l1, 
                total_tile_info.m_num_checkin_blocked_miss_for_read_instr_at_remote_l1, 
                total_tile_info.m_num_checkin_blocked_miss_for_write_instr_at_remote_l1,
                total_tile_info.m_num_hits_for_local_l2_read_instr,
                total_tile_info.m_num_hits_for_local_l2_write_instr,
                total_tile_info.m_num_true_misses_for_read_instr_at_local_l2,
                total_tile_info.m_num_true_misses_for_write_instr_at_local_l2,
                total_tile_info.m_num_ts_blocked_miss_for_write_instr_at_local_l2,
                total_tile_info.m_num_checkin_blocked_miss_for_read_instr_at_local_l2,
                total_tile_info.m_num_checkin_blocked_miss_for_write_instr_at_local_l2,
                total_tile_info.m_num_hits_for_remote_l2_read_instr,
                total_tile_info.m_num_hits_for_remote_l2_write_instr,
                total_tile_info.m_num_true_misses_for_read_instr_at_remote_l2,
                total_tile_info.m_num_true_misses_for_write_instr_at_remote_l2,
                total_tile_info.m_num_ts_blocked_miss_for_write_instr_at_remote_l2,
                total_tile_info.m_num_checkin_blocked_miss_for_read_instr_at_remote_l2,
                total_tile_info.m_num_checkin_blocked_miss_for_write_instr_at_remote_l2);

    out << str << endl;

    out << endl;

    sprintf(str, "[S1S2LCC Latency Breakdown ]\n"
            "    total-memory-serialization %ld\n"
            "    total-outstanding-CAT-serialization %ld\n"
            "    total-outstanding-CAT-operation %ld\n"
            "    total-outstanding-L1-serialization %ld\n"
            "    total-outstanding-L1-operation %ld\n"
            "    total-outstanding-L1-blocked-by-checked-out-write-copy %ld\n"
            "    total-outstanding-L1-blocked-by-unexpired-timestamp %ld\n"
            "    total-REMOTE-request-network-and-serialization %ld\n"
            "    total-REMOTE-reply-network-and-serialization %ld\n"
            "    total-REMOTE-check-in-network-and-serialization %ld\n"
            "    total-outstanding-L2-serialization %ld\n"
            "    total-outstanding-L2-operation %ld\n"
            "    total-outstanding-L2-blocked-by-checked-out-write-copy %ld\n"
            "    total-outstanding-L2-blocked-by-unexpired-timestamp %ld\n"
            "    total-outstanding-DRAMctrl-request-network-and-serialization %ld\n"
            "    total-outstanding-DRAMctrl-reply-network-and-serialization %ld\n"
            "    total-outstanding-DRAM-operation %ld\n"
            "    total-outstanding-bypass-operation %ld\n"
            "    total-migration_latency %ld\n"
            "    \n"
            "    (L1 serialization & operation categories)\n"
            "        L1-cost-for-local-bypass %ld\n"
            "        L1-cost-for-local-hits %ld\n"
            "        L1-cost-for-local-misses %ld\n"
            "        L1-cost-for-local-updates %ld\n"
            "        L1-cost-for-local-block-by-checked-out %ld\n"
            "        L1-cost-for-local-block-by-timestamp %ld\n"
            "        L1-cost-for-remote-bypass %ld\n"
            "        L1-cost-for-remote-hits %ld\n"
            "        L1-cost-for-remote-misses %ld\n"
            "        L1-cost-for-remote-updates %ld\n"
            "        L1-cost-for-remote-block-by-checked-out %ld\n"
            "        L1-cost-for-remote-block-by-timestamp %ld\n"
            "    (L2 serialization & operation categories)\n"
            "        L2-cost-for-local-hits %ld\n"
            "        L2-cost-for-local-misses %ld\n"
            "        L2-cost-for-local-updates %ld\n"
            "        L2-cost-for-local-writebacks %ld\n"
            "        L2-cost-for-local-block-by-checked-out %ld\n"
            "        L2-cost-for-local-block-by-timestamp %ld\n"
            "        L2-cost-for-remote-hits %ld\n"
            "        L2-cost-for-remote-misses %ld\n"
            "        L2-cost-for-remote-updates %ld\n"
            "        L2-cost-for-remote-writebacks %ld\n"
            "        L2-cost-for-remote-block-by-checked-out %ld\n"
            "        L2-cost-for-remote-block-by-timestamp %ld\n",
            total_tile_info.m_total_per_mem_instr_info.m_mem_srz,
            total_tile_info.m_total_per_mem_instr_info.m_cat_srz,
            total_tile_info.m_total_per_mem_instr_info.m_cat_ops,
            total_tile_info.m_total_per_mem_instr_info.m_l1_srz,
            total_tile_info.m_total_per_mem_instr_info.m_l1_ops,
            total_tile_info.m_total_per_mem_instr_info.m_l1_blk_by_checkin,
            total_tile_info.m_total_per_mem_instr_info.m_l1_blk_by_ts,
            total_tile_info.m_total_per_mem_instr_info.m_remote_req_nas,
            total_tile_info.m_total_per_mem_instr_info.m_remote_rep_nas,
            total_tile_info.m_total_per_mem_instr_info.m_remote_checkin_nas,
            total_tile_info.m_total_per_mem_instr_info.m_l2_srz,
            total_tile_info.m_total_per_mem_instr_info.m_l2_ops,
            total_tile_info.m_total_per_mem_instr_info.m_l2_blk_by_checkin,
            total_tile_info.m_total_per_mem_instr_info.m_l2_blk_by_ts,
            total_tile_info.m_total_per_mem_instr_info.m_dramctrl_req_nas,
            total_tile_info.m_total_per_mem_instr_info.m_dramctrl_rep_nas,
            total_tile_info.m_total_per_mem_instr_info.m_dram_ops,
            total_tile_info.m_total_per_mem_instr_info.m_bypass,
            total_tile_info.m_total_per_mem_instr_info.m_mig,
            total_tile_info.m_total_per_mem_instr_info.m_local_bypass,
            total_tile_info.m_total_per_mem_instr_info.m_local_l1_cost_for_hit,
            total_tile_info.m_total_per_mem_instr_info.m_local_l1_cost_for_miss,
            total_tile_info.m_total_per_mem_instr_info.m_local_l1_cost_for_update,
            total_tile_info.m_total_per_mem_instr_info.m_local_l1_blk_by_checkin,
            total_tile_info.m_total_per_mem_instr_info.m_local_l1_blk_by_ts,
            total_tile_info.m_total_per_mem_instr_info.m_remote_bypass,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l1_cost_for_hit,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l1_cost_for_miss,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l1_cost_for_update,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l1_blk_by_checkin,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l1_blk_by_ts,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_hit,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_miss,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_update,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_writeback,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_blk_by_checkin,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_blk_by_ts,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_hit,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_miss,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_update,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_writeback,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_blk_by_checkin,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_blk_by_ts);

    out << str << endl;

    out << endl;

}
