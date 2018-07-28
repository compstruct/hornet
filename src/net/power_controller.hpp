// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __POWER_CONTROLLER_HPP__
#define __POWER_CONTROLLER_HPP__

#include <map>
#include <iostream>
#include <memory>
#include "logger.hpp"
#include "vcd.hpp"
#include "ingress.hpp"
#include "egress.hpp"
#include "statistics.hpp"

using namespace std;
using namespace boost;

class power_controller {
public:
    explicit power_controller(node_id id, std::shared_ptr<tile_statistics> stats,
                              std::shared_ptr<vcd_writer> vcd, logger &log);
    const node_id &get_id() const;
    void add_ingress(node_id src, std::shared_ptr<ingress> ingress);
    void add_egress(node_id dst, std::shared_ptr<egress> egress);
    void adjust_power();
private:
    const node_id id;
    typedef map<node_id, std::shared_ptr<ingress> > ingresses_t;
    ingresses_t ingresses;
    std::shared_ptr<tile_statistics> stats;
    std::shared_ptr<vcd_writer> vcd;
    logger &log;
};

#endif // __POWER_CONTROLLER_HPP__

