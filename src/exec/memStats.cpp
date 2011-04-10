// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memStats.hpp"

memStatsPerTile::memStatsPerTile(uint32_t id, const uint64_t &t) :
    m_id(id), system_time(t) { } 

memStatsPerTile::~memStatsPerTile() {}

memStats::memStats(const uint64_t &t) : aux_statistics(t) {}

memStats::~memStats() {}

void memStats::add_per_tile_stats(shared_ptr<memStatsPerTile> stats) {
    assert(m_per_tile_stats.count(stats->m_id) == 0);
    m_per_tile_stats[stats->m_id] = stats;
}

void memStats::print_stats(ostream &out) {

    out << endl;

    out << "Memory Statistics" << endl;
    out << "-----------------" << endl;
    out << endl;

    uint64_t total_reads = 0;
    uint64_t total_writes = 0;
    uint64_t total_mem = 0;
    double total_read_latency = 0.0;
    double total_write_latency = 0.0;
    double total_latency = 0.0;

    perTileStats_t::iterator it;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        uint32_t id = it->first;
        shared_ptr<memStatsPerTile> st = it->second;
        uint64_t reads = st->m_read_latencies.sample_count();
        uint64_t writes = st->m_write_latencies.sample_count();
        uint64_t mem = reads + writes;
        total_reads += reads;
        total_writes += writes;
        total_mem += mem;
        double amrl = st->m_read_latencies.get_mean();
        double amwl = st->m_write_latencies.get_mean();
        double aml = (amrl*reads + amwl*writes) / mem;
        total_read_latency += amrl * reads;
        total_write_latency += amwl * writes;
        total_latency += total_read_latency + total_write_latency;

        char str[1024];
        sprintf(str, "[Memory %4d ] accesses: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f",
                id, mem, reads, writes, aml, amrl, amwl);

        out << str << endl;

    }
    out << endl;
    char str[1024];
    sprintf(str, "[Summary: Memory ] accesses: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f",
            total_mem, total_reads, total_writes,
            total_latency/total_mem, 
            total_read_latency/total_reads, 
            total_write_latency/total_writes);
    out << str << endl;

    out << endl;
}
