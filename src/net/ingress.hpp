// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __INGRESS_HPP__
#define __INGRESS_HPP__

#include <set>
#include <map>
#include <boost/shared_ptr.hpp>
#include "virtual_queue.hpp"
#include "router.hpp"
#include "logger.hpp"
#include "ingress_id.hpp"

using namespace std;
using namespace boost;

class ingress {
public:
    typedef map<virtual_queue_id, shared_ptr<virtual_queue> > queues_t;
public:
    explicit ingress(const ingress_id &id, const set<virtual_queue_id> &vq_ids,
                     unsigned flits_per_queue, shared_ptr<router> rt,
                     shared_ptr<channel_alloc> virtual_channel_alloc,
                     shared_ptr<pressure_tracker> pressures,
                     logger &log) throw(err);
    void add_queue(shared_ptr<virtual_queue> q) throw(err);
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
    const queues_t &get_queues() const throw();
    const ingress_id &get_id() const throw();
    friend ostream &operator<<(ostream &out, const ingress &v);
private:
    typedef map<virtual_queue_id, virtual_queue_node_id > next_hops_t;
    const ingress_id id;
    queues_t vqs;
    next_hops_t next_hops;
    logger &log;
private:
    ingress(); // not implemented
    ingress(const ingress &); // not implemented
};

inline const ingress_id &ingress::get_id() const throw() { return id; }

inline const ingress::queues_t &ingress::get_queues() const throw() {
    return vqs;
}

ostream &operator<<(ostream &out, const ingress &v);

#endif // __INGRESS_HPP__
