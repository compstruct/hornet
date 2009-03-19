// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRESSURE_TRACKER_HPP__
#define __PRESSURE_TRACKER_HPP__

#include <map>
#include "error.hpp"
#include "logger.hpp"
#include "node_id.hpp"

class pressure_tracker {
public:
    typedef unsigned pressure_t;
public:
    pressure_tracker(const node_id &parent_node, logger &log) throw();
    const node_id &get_id() const throw();
    void inc(const node_id &target) throw(err);
    void dec(const node_id &target) throw(err);
    pressure_t get(const node_id &target) throw(err);
    void add_egress(const node_id &target) throw(err);
private:
    const node_id id;
    map<node_id, pressure_t> pressures;
    logger &log;
};

inline const node_id &pressure_tracker::get_id() const throw() { return id; }

#endif // __PRESSURE_TRACKER_HPP__
