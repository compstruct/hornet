// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cmath>
#include <iomanip>
#include "arbiter.hpp"

arbiter::arbiter(shared_ptr<node> src, shared_ptr<node> dst, arbitration_t sch,
                 shared_ptr<logger> new_log) throw(err)
    : scheme(sch), src_id(src->get_id()), dst_id(dst->get_id()),
      src_to_dst(src->get_link_to(dst->get_id())),
      dst_to_src(dst->get_link_to(src->get_id())), log(new_log) {
    if (sch != AS_NONE && sch != AS_DUMB)
        throw err_bad_arb_scheme(sch);
    if (sch != AS_NONE) {
        log << verbosity(3) << "arbiter " << hex << setfill('0')
            << setw(2) << src_id << "<->" << setw(2) << dst_id << " created"
            << "with bandwidths ->" << dec << src_to_dst->get_bandwidth()
            << " and <-" << dst_to_src->get_bandwidth() << endl;
    }
}

arbiter::~arbiter() throw() { }

void arbiter::tick_positive_edge() throw(err) {
    double src_to_dst_pressure = src_to_dst->get_pressure();
    double dst_to_src_pressure = dst_to_src->get_pressure();
    if (src_to_dst_pressure == 0 && dst_to_src_pressure == 0) return;
    unsigned src_to_dst_bw = src_to_dst->get_bandwidth();
    unsigned dst_to_src_bw = dst_to_src->get_bandwidth();
    unsigned total_bw = src_to_dst_bw + dst_to_src_bw;
    if (total_bw == 0) return;
    double out_pressure_frac =
        src_to_dst_pressure / (src_to_dst_pressure + dst_to_src_pressure);
    double out_bw_frac = (static_cast<double>(src_to_dst_bw) /
                          static_cast<double>(total_bw));
    unsigned new_src_to_dst_bw, new_dst_to_src_bw;
    if (out_pressure_frac > out_bw_frac) {
        new_src_to_dst_bw = floor(total_bw * out_pressure_frac);
        new_dst_to_src_bw = total_bw - new_src_to_dst_bw;
    } else {
        new_src_to_dst_bw = ceil(total_bw * out_pressure_frac);
        new_dst_to_src_bw = total_bw - new_src_to_dst_bw;
    }
    if (new_src_to_dst_bw != src_to_dst_bw) {
        log << verbosity(3) << "[arbiter " << hex << setfill('0')
            << setw(2) << src_id << "<->" << setw(2) << dst_id
            << "] adjusting bandwidths to ->"
            << new_src_to_dst_bw << " and <-" << new_dst_to_src_bw
            << " (target ratio: "
            << src_to_dst->get_pressure()/dst_to_src->get_pressure()
            << ", effective ratio: "
            << (static_cast<double>(new_src_to_dst_bw) /
                static_cast<double>(new_dst_to_src_bw)) << ")" << endl;
        src_to_dst->set_bandwidth(new_src_to_dst_bw);
        dst_to_src->set_bandwidth(new_dst_to_src_bw);
    }
}

void arbiter::tick_negative_edge() throw(err) { }
