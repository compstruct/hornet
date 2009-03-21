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

using namespace std;
using namespace boost;

class ingress_id {
public:
    explicit ingress_id(const node_id parent, const string &name) throw();
    bool operator==(const ingress_id &) const throw();
    bool operator<(const ingress_id &) const throw();
    friend ostream &operator<<(ostream &, const ingress_id &);
    const node_id &get_node_id() const throw();
    const string &get_name() const throw();
private:
    const node_id parent;
    const string name;
private:
    explicit ingress_id() throw(); // not implemented
};

inline ingress_id::ingress_id(const node_id new_parent,
                              const string &new_name) throw()
    : parent(new_parent), name(new_name) { }

inline const node_id &ingress_id::get_node_id() const throw() { return parent; }

inline const string &ingress_id::get_name() const throw() { return name; }

inline bool ingress_id::operator==(const ingress_id &o) const throw() {
    return parent == o.parent && name == o.name;
}

inline bool ingress_id::operator<(const ingress_id &o) const throw() {
    return parent < o.parent || (parent == o.parent && name < o.name);
}

ostream &operator<<(ostream &, const ingress_id &);


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
    typedef map<virtual_queue_id, pair<node_id, virtual_queue_id> > next_hops_t;
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

