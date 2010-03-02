// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __TILE_HPP__
#define __TILE_HPP__

#include <vector>
#include <boost/shared_ptr.hpp>
#include "cstdint.hpp"
#include "node_id.hpp"
#include "pe.hpp"
#include "node.hpp"
#include "arbiter.hpp"
#include "bridge.hpp"
#include "logger.hpp"
#include "statistics.hpp"

using namespace std;
using namespace boost;

class tile {
public:
    tile(const node_id &id, const uint64_t &init_time,
         shared_ptr<statistics> stats, logger &log) throw();
    void add(shared_ptr<pe> p) throw();
    void add(shared_ptr<bridge> p) throw();
    void add(shared_ptr<node> p) throw();
    void add(shared_ptr<arbiter> a) throw();
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
    bool is_ready_to_offer() const throw();
    bool is_drained() const throw();
    bool work_queued() const throw();
    uint64_t next_pkt_time() const throw();
private:
    typedef vector<shared_ptr<pe> > pes_t;
    typedef vector<shared_ptr<node> > nodes_t;
    typedef vector<shared_ptr<bridge> > bridges_t;
    typedef vector<shared_ptr<arbiter> > arbiters_t;
private:
    const node_id id;
    const uint64_t time;
    pes_t pes;
    nodes_t nodes;
    bridges_t bridges;
    arbiters_t arbiters;
    shared_ptr<statistics> stats;
    logger &log;
};

#endif // __TILE_HPP__
