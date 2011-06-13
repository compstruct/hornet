// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedLCCStats.hpp"

privateSharedLCCStatsPerTile::privateSharedLCCStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t), m_evict_block_cycles(0), m_record_ideal_timestamp_delta(false),
    /* cost breakdown study */
    m_memory_subsystem_serialization_cost(0),
    m_cat_serialization_cost(0), m_cat_action_cost(0),
    m_l1_serialization_cost(0), m_l1_action_cost(0), 
    m_l2_network_plus_serialization_cost(0), m_l2_action_cost(0), m_l2_write_block_cost(0),
    m_dram_network_plus_serialization_cost(0), m_dram_offchip_network_plus_dram_action_cost(0),
    m_l1_action(0), m_l2_action(0) { }

privateSharedLCCStatsPerTile::~privateSharedLCCStatsPerTile() {}

void privateSharedLCCStatsPerTile::did_read_l1(bool hit) {
    m_l1_read_hits.add(hit? 1:0, 1);
}

void privateSharedLCCStatsPerTile::did_read_l2(bool hit) {
    m_l2_read_hits.add(hit? 1:0, 1);
}

void privateSharedLCCStatsPerTile::did_write_l2(bool hit, uint64_t blocked_cycles) {
    m_l2_write_hits.add(hit? 1:0, 1);
    m_write_block_cycles.add(blocked_cycles, 1);
}

void privateSharedLCCStatsPerTile::could_not_evict_l2() {
    ++m_evict_block_cycles;
}

void privateSharedLCCStatsPerTile::record_ideal_timestamp_delta(maddr_t maddr, uint64_t delta) {
    if (m_record_ideal_timestamp_delta) {
        m_ideal_delta[maddr].add(delta, 1);
        m_ideal_delta_samples[maddr].push_back(delta);
    }
}

void privateSharedLCCStatsPerTile::did_read_cat(bool hit) {
    m_cat_hits.add(hit? 1 : 0, 1);
}

privateSharedLCCStats::privateSharedLCCStats(const uint64_t &t) : memStats(t) {}

privateSharedLCCStats::~privateSharedLCCStats() {}

void privateSharedLCCStats::print_stats(ostream &out) {

    memStats::print_stats(out);

    /* add privateSharedLCC-specific statistics */

    out << endl;
    
    out << "Private-L1 Shared-L2 LCC Statistics" << endl;
    out << "-----------------------------------" << endl;
    out << endl;

    uint64_t total_l2_reads = 0;
    uint64_t total_l2_writes = 0;
    uint64_t total_l2 = 0;
    uint64_t total_l1_reads = 0;
    uint64_t total_cat_lookups = 0;
    running_stats total_evict_block;

    double total_l2_read_hits = 0.0;
    double total_l2_write_hits = 0.0;
    double total_l2_hits = 0.0;
    double total_l1_reads_hits = 0.0;
    double total_cat_hits = 0.0;
    double total_write_block = 0.0;

    running_stats avg_ideal_deltas;
    running_stats avg_ideal_stddevs;
    running_stats avg_ideal_mean_min;
    running_stats avg_ideal_max_mean;
    running_stats avg_ideal_mid_deltas;
    running_stats avg_ideal_mid_stddevs;

    /* cost breakdown study */
    uint64_t total_memory_subsystem_serialization_cost = 0;
    uint64_t total_cat_serialization_cost = 0;
    uint64_t total_cat_action_cost = 0;
    uint64_t total_l1_serialization_cost = 0;
    uint64_t total_l1_action_cost = 0;
    uint64_t total_l2_network_plus_serialization_cost = 0;
    uint64_t total_l2_action_cost = 0;
    uint64_t total_l2_write_block_cost = 0;
    uint64_t total_dram_network_plus_serialization_cost = 0;
    uint64_t total_dram_offchip_network_plus_dram_action_cost = 0;
    uint64_t total_l1_action = 0;
    uint64_t total_l2_action = 0;

    perTileStats_t::iterator it;
    uint32_t num_tiles = 0;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        ++num_tiles;
        uint32_t id = it->first;
        shared_ptr<privateSharedLCCStatsPerTile> st = static_pointer_cast<privateSharedLCCStatsPerTile>(it->second);

        uint64_t l2_reads = st->m_l2_read_hits.sample_count();
        uint64_t l2_writes = st->m_l2_write_hits.sample_count();
        uint64_t l2 = l2_reads + l2_writes;
        uint64_t l1_reads = st->m_l1_read_hits.sample_count();

        total_l2_reads += l2_reads;
        total_l2_writes += l2_writes;
        total_l2 += l2;
        total_l1_reads += l1_reads;
        total_evict_block.add(st->m_evict_block_cycles, 1);

        double l2_read_rate = st->m_l2_read_hits.get_mean();
        double l2_read_hits = l2_read_rate * l2_reads;
        double l2_write_rate = st->m_l2_write_hits.get_mean();
        double l2_write_hits = l2_write_rate * l2_writes;
        double l2_hits = l2_read_hits + l2_write_hits;
        double l2_rate = l2_hits / l2;
        double l1_read_rate = st->m_l1_read_hits.get_mean();
        double l1_read_hits = l1_read_rate * l1_reads;
        total_l2_read_hits += l2_read_hits;
        total_l2_write_hits += l2_write_hits;
        total_l2_hits += l2_hits;
        total_l1_reads_hits += l1_read_hits;

        uint64_t cat_lookups = st->m_cat_hits.sample_count();
        double cat_rate = st->m_cat_hits.get_mean();
        double cat_hits = cat_rate * cat_lookups;
        total_cat_lookups += cat_lookups;
        total_cat_hits += cat_hits;

        double avg_write_block = st->m_write_block_cycles.get_mean();
        double write_blocks = avg_write_block * l2_writes;
        total_write_block += write_blocks;

        /* cost breakdown study */
        total_memory_subsystem_serialization_cost += st->m_memory_subsystem_serialization_cost;
        total_cat_serialization_cost += st->m_cat_serialization_cost;
        total_cat_action_cost += st->m_cat_action_cost;
        total_l1_serialization_cost += st->m_l1_serialization_cost;
        total_l1_action_cost += st->m_l1_action_cost;
        total_l2_network_plus_serialization_cost += st->m_l2_network_plus_serialization_cost;
        total_l2_action_cost += st->m_l2_action_cost;
        total_l2_write_block_cost += st->m_l2_write_block_cost;
        total_dram_network_plus_serialization_cost += st->m_dram_network_plus_serialization_cost;
        total_dram_offchip_network_plus_dram_action_cost += st->m_dram_offchip_network_plus_dram_action_cost;
        total_l1_action += st->m_l1_action;
        total_l2_action += st->m_l2_action;

        char str[1024];
        sprintf(str, "[Private-shared-LCC %4d ] L1-readhit%%: %.2f L2hit%% %.2f read%%: %.2f write%%: %.2f "
                     "avg write block: %.2f evict block: %ld cyc cat%%: %.2f",
                     id, 100.0*l1_read_rate, 100.0*l2_rate, 100.0*l2_read_rate, 100.0*l2_write_rate, 
                     avg_write_block, st->m_evict_block_cycles, 100.0*cat_rate);

        out << str << endl;

        if (st->m_record_ideal_timestamp_delta) {
            for(map<maddr_t, running_stats>::iterator it = st->m_ideal_delta.begin(); it != st->m_ideal_delta.end(); ++it) {
                maddr_t maddr = it->first;
                avg_ideal_deltas.add(it->second.get_mean(), 1);
                avg_ideal_stddevs.add(it->second.get_std_dev(), 1);
                avg_ideal_mean_min.add(it->second.get_mean() - it->second.get_min(), 1);
                avg_ideal_max_mean.add(it->second.get_mean() - it->second.get_min(), 1);
                sort(st->m_ideal_delta_samples[maddr].begin(), st->m_ideal_delta_samples[maddr].end());
                running_stats middle_90;
                for (uint32_t i = (uint32_t)(st->m_ideal_delta_samples[maddr].size()*0.05); 
                     i < (uint32_t)(st->m_ideal_delta_samples[maddr].size()*0.95); ++i) 
                {
                    middle_90.add(st->m_ideal_delta_samples[maddr][i], 1);
                }
                avg_ideal_mid_deltas.add(middle_90.get_mean(), 1);
                avg_ideal_mid_stddevs.add(middle_90.get_std_dev(), 1);
            }
        }
    }

    out << endl;

    char str[1024];
    sprintf(str, "[Summary: Private-shared-LCC ] L1-readhit%%: %.2f L2hit%% %.2f read%%: %.2f write%%: %.2f "
            "avg write block: %.2f cyc avg evict block : %.2f cyc cat%%: %.2f L1act: %ld L2act: %ld",
            100.0*total_l1_reads_hits/total_l1_reads,
            100.0*total_l2_hits/total_l2,
            100.0*total_l2_read_hits/total_l2_reads,
            100.0*total_l2_write_hits/total_l2_writes,
            total_write_block/total_l2_writes, total_evict_block.get_mean(),100.0*total_cat_hits/total_cat_lookups,
            total_l1_action, total_l2_action);

    out << str << endl;

    if (avg_ideal_deltas.sample_count()) {
        sprintf(str, "[Ideal-Summary: Private-shared-LCC ] avg delta: %.2f stddev: %.2f / avg stddev in line: %.2f "
                     " / avg mid90 delta: %.2f stddev: %.2f / avg mid90 stddev in line : %.2f / avg mean-min: %.2f / avg max-mean: %.2f", 
                     avg_ideal_deltas.get_mean(), avg_ideal_deltas.get_std_dev(), avg_ideal_stddevs.get_mean(),
                     avg_ideal_mid_deltas.get_mean(), avg_ideal_mid_deltas.get_std_dev(), avg_ideal_mid_stddevs.get_mean(), 
                     avg_ideal_mean_min.get_mean(), avg_ideal_max_mean.get_mean());
        out << str << endl;
    }

    /* cost breakdown study */
    sprintf(str, "[Cost] MEM-S: %ld L1-N&S: %ld L1: %ld CAT-S: %ld CAT: %ld L2-N&S: %ld L2-Block: %ld "
                 "L2: %ld DRAM-N&S: %ld DRAM-Offchip: %ld", total_memory_subsystem_serialization_cost,
                 total_l1_serialization_cost, total_l1_action_cost, 
                 total_cat_serialization_cost, total_cat_action_cost,
                 total_l2_network_plus_serialization_cost, total_l2_write_block_cost, total_l2_action_cost,
                 total_dram_network_plus_serialization_cost, total_dram_offchip_network_plus_dram_action_cost);

    out << str << endl;

    out << endl;

}
