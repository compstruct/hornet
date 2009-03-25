// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __EGRESS_HPP__
#define __EGRESS_HPP__

#include <boost/shared_ptr.hpp>
#include "flit.hpp"
#include "ingress.hpp"

using namespace std;
using namespace boost;

class egress_id {
public:
    explicit egress_id(const node_id parent, const string &name) throw();
    bool operator==(const egress_id &) const throw();
    bool operator<(const egress_id &) const throw();
    friend ostream &operator<<(ostream &, const egress_id &);
private:
    const node_id parent;
    const string name;
private:
    explicit egress_id() throw(); // not implemented
};

inline egress_id::egress_id(const node_id p, const string &n) throw()
    : parent(p), name(n) { }

class egress {
public:
    explicit egress(egress_id id, shared_ptr<ingress> target,
                    shared_ptr<pressure_tracker> pressures,
                    unsigned max_bandwidth, logger &log) throw(err);
    const egress_id &get_id() const throw();
    unsigned get_bandwidth() const throw();
    void set_bandwidth(unsigned new_bandwidth) throw();
    double get_pressure() const throw();
    const ingress::queues_t &get_remote_queues() const throw();
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
private:
    const egress_id id;
    const node_id target_id;
    shared_ptr<ingress> target;
    shared_ptr<pressure_tracker> pressures;
    unsigned bandwidth;
    logger &log;
};

#endif // __EGRESS_HPP__
