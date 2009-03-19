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
#include "cpu.hpp"
#include "node.hpp"
#include "arbiter.hpp"
#include "bridge.hpp"

using namespace std;
using namespace boost;

typedef enum {
    RT_STATIC = 0,
    NUM_RTS
} routing_type_t;

ostream &operator<<(ostream &, const routing_type_t &);

class sys {
public:
    sys(shared_ptr<ifstream> image, logger &log) throw(err);
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
private:
    typedef map<unsigned, shared_ptr<cpu> > cpus_t;
    typedef map<unsigned, shared_ptr<bridge> > bridges_t;
    typedef map<unsigned, shared_ptr<node> > nodes_t;
    typedef map<pair<unsigned, unsigned>, shared_ptr<arbiter> > arbiters_t;

    cpus_t cpus;
    bridges_t bridges;
    nodes_t nodes;
    arbiters_t arbiters;

    uint64_t time;

    logger &log;
};

#endif // __SYS_HPP__

