// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedLCCStats.hpp"

privateSharedLCCStatsPerTile::privateSharedLCCStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t), 
    /* cost breakdown study */
    m_blocks(0), m_blocks_eviction(0),
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

void privateSharedLCCStatsPerTile::did_write_l2(bool hit) {
    m_l2_write_hits.add(hit? 1:0, 1);
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

    double total_l2_read_hits = 0.0;
    double total_l2_write_hits = 0.0;
    double total_l2_hits = 0.0;
    double total_l1_reads_hits = 0.0;
    double total_cat_hits = 0.0;

    running_stats avg_ideal_deltas;
    running_stats avg_ideal_stddevs;
    running_stats avg_ideal_mean_min;
    running_stats avg_ideal_max_mean;
    running_stats avg_ideal_mid_deltas;
    running_stats avg_ideal_mid_stddevs;

    uint64_t total_blocks = 0;
    uint64_t total_blocks_eviction = 0;

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

        total_blocks += st->m_blocks;
        total_blocks_eviction += st->m_blocks_eviction;

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
        sprintf(str, "[Private-shared-LCC %4d ] %ld reads %ld writes L1hit%%: %.2f L2hit%% %.2f read%%: %.2f write%%: %.2f "
                     "#blks: %ld (block-evict: %ld) cat%%: %.2f L1ops: %ld L2ops: %ld ",
                     id, l1_reads, l2_writes, 100.0*l1_read_rate, 100.0*l2_rate, 100.0*l2_read_rate, 100.0*l2_write_rate, 
                     st->m_blocks, st->m_blocks_eviction,
                     100.0*cat_rate, st->m_l1_action, st->m_l2_action);

        out << str << endl;

    }

    out << endl;

    char str[1024];
    sprintf(str, "[Summary: Private-shared-LCC ] %ld reads %ld writes L1hit%%: %.2f L2hit%% %.2f read%%: %.2f write%%: %.2f "
            "#blks: %ld (block-evict: %ld) cat%%: %.2f L1ops: %ld L2ops: %ld ",
            total_l1_reads, total_l2_writes,
            100.0*total_l1_reads_hits/total_l1_reads,
            100.0*total_l2_hits/total_l2,
            100.0*total_l2_read_hits/total_l2_reads,
            100.0*total_l2_write_hits/total_l2_writes,
            total_blocks, total_blocks_eviction,
            100.0*total_cat_hits/total_cat_lookups,
            total_l1_action, total_l2_action );

    out << str << endl;

    /* cost breakdown study */
    sprintf(str, "[Latency Breakdown ] MEM-S: %ld L1-N&S: %ld L1: %ld CAT-S: %ld CAT: %ld L2-N&S: %ld L2-Block: %ld "
                 "L2: %ld DRAM-N&S: %ld DRAM-Offchip: %ld", total_memory_subsystem_serialization_cost,
                 total_l1_serialization_cost, total_l1_action_cost, 
                 total_cat_serialization_cost, total_cat_action_cost,
                 total_l2_network_plus_serialization_cost, total_l2_write_block_cost, total_l2_action_cost,
                 total_dram_network_plus_serialization_cost, total_dram_offchip_network_plus_dram_action_cost);

    out << str << endl;

    out << endl;

}