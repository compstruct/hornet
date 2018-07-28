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
#include "vcd.hpp"
#include "logger.hpp"
#include "random.hpp"

class crossbar {
public:
    crossbar(node_id parent, std::shared_ptr<tile_statistics> stats,
             std::shared_ptr<vcd_writer> vcd, logger &log,
             std::shared_ptr<random_gen> ran);
    void add_ingress(node_id src, std::shared_ptr<ingress> ingress);
    void add_egress(node_id dst, std::shared_ptr<egress> egress);
    void tick_positive_edge();
    void tick_negative_edge();
private:
    const node_id &get_id();
    void rebuild_queues();
private:
    const node_id id;

    typedef map<node_id, std::shared_ptr<ingress> > ingresses_t;
    typedef map<node_id, std::shared_ptr<egress> > egresses_t;
    ingresses_t ingresses;
    egresses_t egresses;
    typedef vector<std::tuple<node_id, std::shared_ptr<virtual_queue> > > nvqs_t;
    nvqs_t ingress_qs;
    typedef vector<std::shared_ptr<virtual_queue> > vqs_t;
    vqs_t egress_qs;
    std::shared_ptr<tile_statistics> stats;
    std::shared_ptr<vcd_writer> vcd;
    logger &log;
    std::shared_ptr<random_gen> ran;
    typedef struct {
        char v_xbar_demand;
        char v_xbar_use;
    } vcd_hooks_t;
    vcd_hooks_t vcd_hooks;
};

inline const node_id &crossbar::get_id() { return id; }

#endif // __CROSSBAR_HPP__

