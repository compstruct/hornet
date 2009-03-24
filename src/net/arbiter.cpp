// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cmath>
#include <iomanip>
#include "arbiter.hpp"

arbiter::arbiter(uint64_t &time, shared_ptr<node> src, shared_ptr<node> dst,
                 arbitration_t new_sch, unsigned new_min_bw, unsigned new_period,
                 logger &l) throw(err)
    : system_time(time), scheme(new_sch), min_bw(new_min_bw), period(new_period),
      next_arb(time), src_to_dst(src->get_egress_to(dst->get_id())),
      dst_to_src(dst->get_egress_to(src->get_id())), log(l) {
    if (scheme != AS_NONE && scheme != AS_DUMB)
        throw err_bad_arb_scheme(scheme);
    if (src_to_dst->get_bandwidth() + dst_to_src->get_bandwidth() < 2 * min_bw) {
        throw err_bad_arb_min_bw(src->get_id().get_numeric_id(),
                                 dst->get_id().get_numeric_id(),
                                 src_to_dst->get_bandwidth(),
                                 dst_to_src->get_bandwidth(), min_bw);
    }
    if (scheme != AS_NONE) {
        LOG(log,3) << "arbiter " << hex << setfill('0')
                   << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                   << " created with bandwidths ->" << dec
                   << src_to_dst->get_bandwidth()
                   << " and <-" << dst_to_src->get_bandwidth() << endl;
    }
}

void arbiter::tick_positive_edge() throw(err) {
    if (next_arb != system_time) {
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                    << "] waiting until tick " << dec << next_arb << endl;
        return;
    }
    next_arb += period;
    double src_to_dst_pressure = src_to_dst->get_pressure();
    double dst_to_src_pressure = dst_to_src->get_pressure();
    if (src_to_dst_pressure == 0 && dst_to_src_pressure == 0) {
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                    << "] has no pressures to arbitrate" << endl;
        return;
    }
    unsigned src_to_dst_bw = src_to_dst->get_bandwidth();
    unsigned dst_to_src_bw = dst_to_src->get_bandwidth();
    unsigned total_bw = src_to_dst_bw + dst_to_src_bw;
    if (total_bw == 0) {
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                    << "] has no bandwidth to allocate" << endl;
        return;
    }
    double out_pressure_frac =
        src_to_dst_pressure / (src_to_dst_pressure + dst_to_src_pressure);
    double out_bw_frac = (static_cast<double>(src_to_dst_bw) /
                          static_cast<double>(total_bw));
    unsigned new_src_to_dst_bw, new_dst_to_src_bw;
    if (out_pressure_frac > out_bw_frac) {
        new_src_to_dst_bw = floor(total_bw * out_pressure_frac);
        if (new_src_to_dst_bw < min_bw)
            new_src_to_dst_bw = min_bw;
        if (new_src_to_dst_bw > total_bw - min_bw)
            new_src_to_dst_bw = total_bw - min_bw;
        new_dst_to_src_bw = total_bw - new_src_to_dst_bw;
    } else {
        new_src_to_dst_bw = ceil(total_bw * out_pressure_frac);
        if (new_src_to_dst_bw < min_bw)
            new_src_to_dst_bw = min_bw;
        if (new_src_to_dst_bw > total_bw - min_bw)
            new_src_to_dst_bw = total_bw - min_bw;
        new_dst_to_src_bw = total_bw - new_src_to_dst_bw;
    }
    assert(new_src_to_dst_bw >= min_bw);
    assert(new_dst_to_src_bw >= min_bw);
    LOG(log,12) << "[arbiter " << hex << setfill('0')
                << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                << "] pressures: -> " << dec << fixed << setprecision(4)
                << src_to_dst_pressure << ", <- " << dst_to_src_pressure
                << " (target ratio: "
                << src_to_dst->get_pressure()/dst_to_src->get_pressure()
                << ", effective ratio: "
                << (static_cast<double>(new_src_to_dst_bw) /
                    static_cast<double>(new_dst_to_src_bw)) << ")" << endl;
    if (new_src_to_dst_bw != src_to_dst_bw) {
        LOG(log,2) << "[arbiter " << hex << setfill('0')
            << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
            << "] adjusting bandwidths to ->" << dec
            << new_src_to_dst_bw << " and <-" << new_dst_to_src_bw << "\n"
            << "[arbiter " << hex << setfill('0')
            << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
            << "]     (target ratio: " << dec << fixed << setprecision(4)
            << src_to_dst->get_pressure()/dst_to_src->get_pressure()
            << ", effective ratio: "
            << (static_cast<double>(new_src_to_dst_bw) /
                static_cast<double>(new_dst_to_src_bw)) << ")" << endl;
        src_to_dst->set_bandwidth(new_src_to_dst_bw);
        dst_to_src->set_bandwidth(new_dst_to_src_bw);
    }
}

void arbiter::tick_negative_edge() throw(err) { }
