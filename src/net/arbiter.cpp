// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cmath>
#include <climits>
#include <iomanip>
#include "arbiter.hpp"

arbiter::arbiter(const uint64_t &time,
                 shared_ptr<node> src, shared_ptr<node> dst,
                 arbitration_t new_sch, unsigned new_min_bw,
                 unsigned new_period, unsigned new_delay,
                 shared_ptr<statistics> new_stats,
                 logger &l) throw(err)
    : id(src->get_id(), dst->get_id()),
      system_time(time), scheme(new_sch), min_bw(new_min_bw),
      period(new_period), delay(new_delay), next_arb(time), arb_queue(),
      src_to_dst(src->get_egress_to(dst->get_id())),
      dst_to_src(dst->get_egress_to(src->get_id())),
      total_bw(src_to_dst->get_bandwidth() + dst_to_src->get_bandwidth()),
      last_queued_src_to_dst_bw(src_to_dst->get_bandwidth()),
      stats(new_stats), log(l) {
    if (scheme != AS_NONE && scheme != AS_DUMB)
        throw err_bad_arb_scheme(scheme);
    if (total_bw < 2 * min_bw) {
        throw err_bad_arb_min_bw(src->get_id().get_numeric_id(),
                                 dst->get_id().get_numeric_id(),
                                 src_to_dst->get_bandwidth(),
                                 dst_to_src->get_bandwidth(), min_bw);
    }
    stats->register_links(src_to_dst->get_id(), dst_to_src->get_id(),
                          total_bw);
    if (scheme != AS_NONE) {
        LOG(log,3) << "arbiter " << hex << setfill('0')
                   << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                   << " created with bandwidths ->" << dec
                   << src_to_dst->get_bandwidth()
                   << " and <-" << dst_to_src->get_bandwidth() << endl;
    }
}

const link_id &arbiter::get_id() const throw() { return id; }

void arbiter::tick_positive_edge() throw(err) {
    if (next_arb > system_time) {
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                    << "] waiting until tick " << dec << next_arb << endl;
        return;
    }
    next_arb = system_time + period;
    double src_to_dst_pressure = src_to_dst->get_pressure();
    double dst_to_src_pressure = dst_to_src->get_pressure();
    unsigned src_to_dst_bw = last_queued_src_to_dst_bw;
    unsigned dst_to_src_bw = total_bw - src_to_dst_bw;
    if (src_to_dst_pressure == 0 && dst_to_src_pressure == 0) {
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                    << "] has no pressures to arbitrate" << endl;
    } else if (total_bw == 0) {
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                    << "] has no bandwidth to allocate" << endl;
    } else if ((src_to_dst_bw >= src_to_dst_pressure)
               && (dst_to_src_bw >= dst_to_src_pressure)) {
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                    << "] pressures satisfied with last configuration"
                    << endl;
    } else {
        unsigned num_src_queues = 0, num_dst_queues = 0;
        const ingress::queues_t &src_qs = dst_to_src->get_remote_queues();
        for (ingress::queues_t::const_iterator qi = src_qs.begin();
             qi != src_qs.end(); ++qi) {
            if (!qi->second->back_is_full()) ++num_src_queues;
        }
        const ingress::queues_t &dst_qs = src_to_dst->get_remote_queues();
        for (ingress::queues_t::const_iterator qi = dst_qs.begin();
             qi != dst_qs.end(); ++qi) {
            if (!qi->second->back_is_full()) ++num_dst_queues;
        }
        double out_pressure_frac =
            src_to_dst_pressure / (src_to_dst_pressure + dst_to_src_pressure);
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->"
                    << dst_to_src->get_id()
                    << "] pressures: ->" << dec
                    << fixed << setprecision(4)
                    << src_to_dst_pressure << ", <-"
                    << dst_to_src_pressure << endl;
        LOG(log,12) << "[arbiter " << hex << setfill('0')
                    << src_to_dst->get_id() << "<->"
                    << dst_to_src->get_id()
                    << "] available queues: ->" << num_dst_queues
                    << " <-" << num_src_queues << endl;
        double out_bw_frac = (static_cast<double>(src_to_dst_bw) /
                              static_cast<double>(total_bw));
        unsigned new_src_to_dst_bw, new_dst_to_src_bw;
        if (out_pressure_frac > out_bw_frac) {
            new_src_to_dst_bw = floor(total_bw * out_pressure_frac);
        } else {
            new_src_to_dst_bw = ceil(total_bw * out_pressure_frac);
        }
        if (new_src_to_dst_bw < min_bw)
            new_src_to_dst_bw = min_bw;
        if (new_src_to_dst_bw > total_bw - min_bw)
            new_src_to_dst_bw = total_bw - min_bw;
        while ((new_src_to_dst_bw > min_bw)
               && (new_src_to_dst_bw > num_dst_queues)
               && (total_bw - new_src_to_dst_bw < num_src_queues)
               && (total_bw - new_src_to_dst_bw < dst_to_src_pressure))
            --new_src_to_dst_bw;
        while ((total_bw - new_src_to_dst_bw > min_bw)
               && (total_bw - new_src_to_dst_bw > num_src_queues)
               && (new_src_to_dst_bw < num_dst_queues)
               && (new_src_to_dst_bw < src_to_dst_pressure))
            ++new_src_to_dst_bw;
        new_dst_to_src_bw = total_bw - new_src_to_dst_bw;
        assert(new_src_to_dst_bw >= min_bw);
        assert(new_dst_to_src_bw >= min_bw);
        if (new_src_to_dst_bw != last_queued_src_to_dst_bw) {
            LOG(log,2) << "[arbiter " << hex << setfill('0')
                << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                << "] new bandwidths ->" << dec
                << new_src_to_dst_bw << " and <-" << new_dst_to_src_bw;
            if (delay > 0) {
                LOG(log,2) << " (" << dec << delay << "-cycle delay)";
            }
            LOG(log,2) << "\n[arbiter " << hex << setfill('0')
                << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
                << "]     (target ratio: " << dec << fixed << setprecision(4)
                << src_to_dst->get_pressure()/dst_to_src->get_pressure()
                << ", effective ratio: "
                << (static_cast<double>(new_src_to_dst_bw) /
                    static_cast<double>(new_dst_to_src_bw)) << ")" << endl;
            arb_queue.push(make_tuple(system_time + delay,
                                      new_src_to_dst_bw, new_dst_to_src_bw));
            last_queued_src_to_dst_bw = new_src_to_dst_bw;
        }
    }
    if (!arb_queue.empty() && arb_queue.front().get<0>() <= system_time) {
        unsigned cur_src_to_dst_bw = src_to_dst->get_bandwidth();
        unsigned new_src_to_dst_bw = arb_queue.front().get<1>();
        unsigned new_dst_to_src_bw = arb_queue.front().get<2>();
        unsigned min_sw_link = min(cur_src_to_dst_bw, new_src_to_dst_bw);
        unsigned num_sw_links = labs(cur_src_to_dst_bw - new_src_to_dst_bw);
        LOG(log,2) << "[arbiter " << hex << setfill('0')
            << src_to_dst->get_id() << "<->" << dst_to_src->get_id()
            << "] setting bandwidths ->" << dec << arb_queue.front().get<1>()
            << " and <-" << arb_queue.front().get<2>() << endl;
        src_to_dst->set_bandwidth(new_src_to_dst_bw);
        dst_to_src->set_bandwidth(new_dst_to_src_bw);
        arb_queue.pop();
        stats->switch_links(src_to_dst->get_id(), dst_to_src->get_id(),
                            min_sw_link, num_sw_links);
    }
}

void arbiter::tick_negative_edge() throw(err) { }
