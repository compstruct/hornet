// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memStats.hpp"

memStatsPerTile::memStatsPerTile(uint32_t id, const uint64_t &t) :
    m_id(id), system_time(t), m_reads(0), m_writes(0) { } 

memStatsPerTile::~memStatsPerTile() {}

memStats::memStats(const uint64_t &t) : aux_statistics(t) {}

memStats::~memStats() {}

void memStats::add_per_tile_stats(std::shared_ptr<memStatsPerTile> stats) {
    assert(m_per_tile_stats.count(stats->m_id) == 0);
    m_per_tile_stats[stats->m_id] = stats;
}

void memStats::print_stats(ostream &out) {

    char str[1024];
    out << endl;

    out << "Memory Statistics" << endl;
    out << "-----------------" << endl;
    out << endl;

    uint64_t total_reads = 0;
    uint64_t total_writes = 0;

    perTileStats_t::iterator it;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        uint32_t id = it->first;
        std::shared_ptr<memStatsPerTile> st = it->second;
        total_reads += st->m_reads;
        total_writes += st->m_writes;

        sprintf(str, "[Memory %d ] %ld %ld %ld",
                id, st->m_reads + st->m_writes, st->m_reads, st->m_writes);

        out << str << endl;

    }

    out << endl;

    sprintf(str, "[Summary: Memory ] accesses: %ld reads: %ld writes: %ld",
            total_reads + total_writes, total_reads, total_writes);
    out << str << endl;

    out << endl;
}
