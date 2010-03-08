// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SYS_HPP__
#define __SYS_HPP__

#include <vector>
#include <map>
#include <utility>
#include <fstream>
#include <boost/shared_ptr.hpp>
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
    sys(const uint64_t &time, shared_ptr<ifstream> image, uint64_t stats_t0,
        shared_ptr<vector<string> > event_files,
        shared_ptr<vcd_writer> vcd,
        logger &log, uint32_t seed, bool use_graphite_inj,
        uint64_t test_flags) throw(err);
    shared_ptr<system_statistics> get_statistics() const throw();
    bool work_tbd_darsim() throw(err);
    bool nothing_to_offer() throw(err);
    uint64_t advance_time() throw(err);
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
    void fast_forward_time(uint64_t new_time) throw();
    bool is_drained() const throw();
    // parallel support
    uint32_t get_num_tiles() const throw();
    void tick_positive_edge_tile(uint32_t tile) throw(err);
    void tick_negative_edge_tile(uint32_t tile) throw(err);
    void fast_forward_time_tile(uint32_t tile, uint64_t new_time) throw();
private:
    typedef vector<shared_ptr<tile> > tiles_t;

private:
    uint64_t sys_time;
    typedef enum {
        TF_RANDOMIZE_NODE_ORDER = 1
    } test_flags_t;

    tiles_t tiles;
    vector<uint32_t> tile_indices;

    shared_ptr<system_statistics> stats;
    logger &log;
    shared_ptr<BoostRand> sys_rand;
    uint64_t test_flags;
};

#endif // __SYS_HPP__
