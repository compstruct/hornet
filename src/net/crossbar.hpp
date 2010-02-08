// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CROSSBAR_HPP__
#define __CROSSBAR_HPP__

#include <vector>
#include "node_id.hpp"
#include "virtual_queue.hpp"
#include "ingress.hpp"
#include "egress.hpp"
#include "statistics.hpp"
#include "logger.hpp"
#include "vcd.hpp"

class crossbar {
public:
    crossbar(node_id parent, shared_ptr<statistics> stats,
             logger &log, shared_ptr<vcd_writer> vcd) throw();
    void add_ingress(node_id src, shared_ptr<ingress> ingress) throw(err);
    void add_egress(node_id dst, shared_ptr<egress> egress) throw(err);
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
private:
    const node_id &get_id() throw();
    void rebuild_queues() throw();
private:
    const node_id id;

    typedef map<node_id, shared_ptr<ingress> > ingresses_t;
    typedef map<node_id, shared_ptr<egress> > egresses_t;
    ingresses_t ingresses;
    egresses_t egresses;
    typedef vector<tuple<node_id, shared_ptr<virtual_queue> > > nvqs_t;
    nvqs_t ingress_qs;
    typedef vector<shared_ptr<virtual_queue> > vqs_t;
    vqs_t egress_qs;
    shared_ptr<statistics> stats;
    logger &log;
    shared_ptr<vcd_writer> vcd;
};

inline const node_id &crossbar::get_id() throw() { return id; }

#endif // __CROSSBAR_HPP__

