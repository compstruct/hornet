// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SYS_HPP__
#define __SYS_HPP__

#include <vector>
#include <map>
#include <utility>
#include <fstream>
#include <memory>
#include <boost/thread.hpp>
#include "cstdint.hpp"
#include "vcd.hpp"
#include "statistics.hpp"
#include "logger.hpp"
#include "tile.hpp"
#include "random.hpp"

using namespace std;
using namespace boost;

class sys {
public:
    sys(const uint64_t &time, std::shared_ptr<ifstream> image, 
        const uint64_t &stats_t0,
        std::shared_ptr<vector<string> > event_files,
        std::shared_ptr<vector<string> > memtrace_files,
        std::shared_ptr<vcd_writer> vcd,
        logger &log, uint32_t seed, bool use_graphite_inj,
        uint64_t test_flags);
    std::shared_ptr<system_statistics> get_statistics() const;
    std::shared_ptr<tile_statistics> get_statistics_tile(tile_id t) const;
    bool work_tbd_darsim();
    bool nothing_to_offer();
    uint64_t advance_time();
    uint64_t get_time() const;
    void tick_positive_edge();
    void tick_negative_edge();
    void fast_forward_time(uint64_t new_time);
    bool is_drained() const;
    // parallel support
    uint32_t get_num_tiles() const;
    uint64_t get_time_tile(tile_id tile) const;
    void tick_positive_edge_tile(tile_id tile);
    void tick_negative_edge_tile(tile_id tile);
    void fast_forward_time_tile(tile_id tile, uint64_t new_time);
    uint64_t advance_time_tile(tile_id tile);
    bool is_drained_tile(tile_id tile) const;
private:
    typedef vector<std::shared_ptr<tile> > tiles_t;

private:
    uint64_t sys_time;
    typedef enum {
        TF_RANDOMIZE_NODE_ORDER = 1
    } test_flags_t;

    tiles_t tiles;
    vector<tile_id> tile_indices;

    std::shared_ptr<system_statistics> stats;
    logger &log;
    std::shared_ptr<random_gen> sys_rand;
    uint64_t test_flags;
};

#endif // __SYS_HPP__
