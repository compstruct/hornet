// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __NODE_HPP__
#define __NODE_HPP__

#include <map>
#include <iostream>
#include <memory>
#include "logger.hpp"
#include "virtual_queue.hpp"
#include "router.hpp"
#include "ingress.hpp"
#include "egress.hpp"
#include "crossbar.hpp"
#include "power_controller.hpp"

using namespace std;
using namespace boost;

class node {
public:
    explicit node(node_id id, uint32_t flits_per_queue,
                  std::shared_ptr<router> new_router,
                  std::shared_ptr<channel_alloc> new_vc_alloc,
                  std::shared_ptr<tile_statistics> stats,
                  std::shared_ptr<vcd_writer> vcd, logger &log,
                  std::shared_ptr<random_gen> ran);
    const node_id &get_id() const;
    void add_ingress(node_id src, std::shared_ptr<ingress> ingress);
    void add_egress(node_id dst, std::shared_ptr<egress> egress);
    void add_queue_id(virtual_queue_id id);
    std::shared_ptr<ingress> get_ingress_from(node_id src);
    std::shared_ptr<egress> get_egress_to(node_id dst);
    std::shared_ptr<router> get_router();
    std::shared_ptr<channel_alloc> get_channel_alloc();
    std::shared_ptr<pressure_tracker> get_pressures();
    void connect_from(const string &port_name,
                      std::shared_ptr<node> src, const string &src_port_name,
                      const set<virtual_queue_id> &vq_ids, unsigned link_bw,
                      unsigned bw_to_xbar);
    void tick_positive_edge();
    void tick_negative_edge();
    bool is_drained() const;
private:
    const node_id id;
    unsigned flits_per_queue;
    typedef map<node_id, std::shared_ptr<ingress> > ingresses_t;
    typedef map<node_id, std::shared_ptr<egress> > egresses_t;
    std::shared_ptr<router> rt;
    std::shared_ptr<channel_alloc> vc_alloc;
    std::shared_ptr<pressure_tracker> pressures;
    ingresses_t ingresses;
    egresses_t egresses;
    crossbar xbar;
    power_controller pwr_ctl;
    set<virtual_queue_id> queue_ids;
    std::shared_ptr<tile_statistics> stats;
    std::shared_ptr<vcd_writer> vcd;
    logger &log;
};

inline const node_id &node::get_id() const { return id; }

#endif // __NODE_HPP__

