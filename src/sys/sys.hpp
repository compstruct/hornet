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

using namespace std;
using namespace boost;

class sys {
public:
    sys(shared_ptr<ifstream> image, uint64_t stats_t0,
        shared_ptr<vector<string> > event_files, logger &log) throw(err);
    shared_ptr<statistics> get_statistics() throw();
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
    bool is_drained() const throw();
private:
    typedef map<unsigned, shared_ptr<pe> > pes_t;
    typedef map<unsigned, shared_ptr<bridge> > bridges_t;
    typedef map<unsigned, shared_ptr<node> > nodes_t;
    typedef map<tuple<unsigned, unsigned>, shared_ptr<arbiter> > arbiters_t;

    pes_t pes;
    bridges_t bridges;
    nodes_t nodes;
    arbiters_t arbiters;

    uint64_t time;
    shared_ptr<statistics> stats;
    logger &log;
};

#endif // __SYS_HPP__

