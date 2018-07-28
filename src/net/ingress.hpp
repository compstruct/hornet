// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __INGRESS_HPP__
#define __INGRESS_HPP__

#include <set>
#include <map>
#include <memory>
#include "virtual_queue.hpp"
#include "vcd.hpp"
#include "statistics.hpp"
#include "logger.hpp"
#include "ingress_id.hpp"

using namespace std;
using namespace boost;

class ingress {
public:
    typedef map<virtual_queue_id, std::shared_ptr<virtual_queue> > queues_t;
public:
    explicit ingress(const ingress_id &id, const node_id &src_node_id,
                     const set<virtual_queue_id> &vq_ids,
                     unsigned flits_per_queue, unsigned bw_to_xbar,
                     std::shared_ptr<channel_alloc> virtual_channel_alloc,
                     std::shared_ptr<pressure_tracker> pressures,
                     std::shared_ptr<tile_statistics> stats,
                     std::shared_ptr<vcd_writer> vcd,
                     logger &log);
    void add_queue(std::shared_ptr<virtual_queue> q);
    void tick_positive_edge();
    void tick_negative_edge();
    bool is_drained() const;
    const queues_t &get_queues() const;
    const ingress_id &get_id() const;
    const node_id &get_src_node_id() const;
    const unsigned get_bw_to_xbar() const;
    friend ostream &operator<<(ostream &out, const ingress &v);
private:
    typedef map<virtual_queue_id, virtual_queue_node_id > next_hops_t;
    const ingress_id id;
    const unsigned bw_to_xbar;
    const node_id src_node_id;
    queues_t vqs;
    next_hops_t next_hops;
    std::shared_ptr<tile_statistics> stats;
    logger &log;
private:
    ingress(); // not implemented
    ingress(const ingress &); // not implemented
};

inline const ingress_id &ingress::get_id() const { return id; }

inline const node_id &ingress::get_src_node_id() const {
    return src_node_id;
}

inline const unsigned ingress::get_bw_to_xbar() const {
    return bw_to_xbar;
}

inline const ingress::queues_t &ingress::get_queues() const {
    return vqs;
}

ostream &operator<<(ostream &out, const ingress &v);

#endif // __INGRESS_HPP__
