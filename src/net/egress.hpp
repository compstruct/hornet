// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __EGRESS_HPP__
#define __EGRESS_HPP__

#include <memory>
#include "flit.hpp"
#include "ingress.hpp"
#include "egress_id.hpp"

using namespace std;
using namespace boost;

class egress {
public:
    explicit egress(egress_id id, std::shared_ptr<ingress> target,
                    std::shared_ptr<pressure_tracker> pressures,
                    unsigned max_bandwidth, logger &log);
    const egress_id &get_id() const;
    const node_id &get_target_id() const;
    unsigned get_bandwidth() const;
    void set_bandwidth(unsigned new_bandwidth);
    double get_pressure() const;
    const ingress::queues_t &get_remote_queues() const;
private:
    const egress_id id;
    const node_id target_id;
    std::shared_ptr<ingress> target;
    std::shared_ptr<pressure_tracker> pressures;
    unsigned bandwidth;
    logger &log;
};

#endif // __EGRESS_HPP__
