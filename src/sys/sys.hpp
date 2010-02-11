// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SYS_HPP__
#define __SYS_HPP__

#include <vector>
#include <map>
#include <utility>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include "cstdint.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "pe.hpp"
#include "node.hpp"
#include "arbiter.hpp"
#include "bridge.hpp"
#include "par_random.hpp"

using namespace std;
using namespace boost;

class sys {
public:
    sys(const uint64_t &time, shared_ptr<ifstream> image, uint64_t stats_t0,
        shared_ptr<vector<string> > event_files,
        shared_ptr<statistics> stats, logger &log,
        uint32_t seed, bool use_graphite_inj) throw(err);
    shared_ptr<statistics> get_statistics() throw();
    bool work_tbd_darsim() throw(err);
    bool nothing_to_offer() throw(err);
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
    bool is_drained() const throw();
private:
    typedef map<unsigned, shared_ptr<BoostRand> > rand_t;
    //typedef map<unsigned, shared_ptr<pe> > pes_t;
    typedef vector<shared_ptr<pe> > pes_t;
    typedef map<unsigned, shared_ptr<bridge> > bridges_t;
    typedef map<unsigned, shared_ptr<node> > nodes_t;
    typedef map<tuple<unsigned, unsigned>, shared_ptr<arbiter> > arbiters_t;

    rand_t rand;
public:
    pes_t pes;
private:
    bridges_t bridges;
    nodes_t nodes;
    arbiters_t arbiters;

    const uint64_t &time;
    shared_ptr<statistics> stats;
    logger &log;
};

#endif // __SYS_HPP__
