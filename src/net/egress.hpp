// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __EGRESS_HPP__
#define __EGRESS_HPP__

#include <boost/shared_ptr.hpp>
#include "flit.hpp"
#include "ingress.hpp"
#include "egress_id.hpp"

using namespace std;
using namespace boost;

class egress {
public:
    explicit egress(egress_id id, shared_ptr<ingress> target,
                    shared_ptr<pressure_tracker> pressures,
                    unsigned max_bandwidth, logger &log) throw(err);
    const egress_id &get_id() const throw();
    const node_id &get_target_id() const throw();
    unsigned get_bandwidth() const throw();
    void set_bandwidth(unsigned new_bandwidth) throw();
    double get_pressure() const throw();
    const ingress::queues_t &get_remote_queues() const throw();
private:
    const egress_id id;
    const node_id target_id;
    shared_ptr<ingress> target;
    shared_ptr<pressure_tracker> pressures;
    unsigned bandwidth;
    logger &log;
};

#endif // __EGRESS_HPP__
