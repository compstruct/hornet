// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <utility>
#include "endian.hpp"
#include "channel_alloc.hpp"
#include "node.hpp"

node::node(node_id new_id, uint32_t memsz, shared_ptr<router> new_rt,
           shared_ptr<channel_alloc> new_vca,
           shared_ptr<statistics> stats, logger &l) throw()
    : id(new_id), flits_per_queue(memsz), rt(new_rt), vc_alloc(new_vca),
      pressures(new pressure_tracker(new_id, l)), ingresses(), egresses(),
      xbar(new_id, stats, l), queue_ids(), log(l) {
    LOG(log,3) << "node " << id << " created with " << dec << memsz
        << " flit" << (memsz == 1 ? "" : "s") << " per queue" << endl;
}

void node::add_queue_id(virtual_queue_id q) throw(err) {
    if (queue_ids.find(q) != queue_ids.end())
        throw err_duplicate_queue(id.get_numeric_id(), q.get_numeric_id());
}

void node::add_ingress(node_id src, shared_ptr<ingress> ingress) throw(err) {
    if (ingresses.find(src) != ingresses.end()) {
        throw err_duplicate_ingress(get_id().get_numeric_id(),
                                    src.get_numeric_id());
    }
    for (ingress::queues_t::const_iterator q = ingress->get_queues().begin();
         q != ingress->get_queues().end(); ++q) {
        add_queue_id(q->first);
    }
    ingresses[src] = ingress;
    vc_alloc->add_ingress(ingress);
    xbar.add_ingress(src, ingress);
}

void node::add_egress(node_id dst, shared_ptr<egress> egress) throw(err) {
    if (egresses.find(dst) != egresses.end()) {
        throw err_duplicate_egress(get_id().get_numeric_id(),
                                   dst.get_numeric_id());
    }
    egresses[dst] = egress;
    vc_alloc->add_egress(egress);
    xbar.add_egress(dst, egress);
}

shared_ptr<ingress> node::get_ingress_from(node_id src) throw(err) {
    if (ingresses.find(src) == ingresses.end())
        throw err_bad_neighbor(get_id().get_numeric_id(), src.get_numeric_id());
    return ingresses[src];
}

shared_ptr<egress> node::get_egress_to(node_id dst) throw(err) {
    if (egresses.find(dst) == egresses.end())
        throw err_bad_neighbor(get_id().get_numeric_id(), dst.get_numeric_id());
    return egresses[dst];
}

shared_ptr<router> node::get_router() throw() { return rt; }

shared_ptr<channel_alloc> node::get_channel_alloc() throw() { return vc_alloc; }

shared_ptr<pressure_tracker> node::get_pressures() throw() { return pressures; }

void node::connect_from(const string &port_name,
                        shared_ptr<node> src, const string &src_port_name,
                        const set<virtual_queue_id> &vq_ids, unsigned link_bw,
                        unsigned bw_to_xbar)
    throw(err) {
    ingress_id dst_id(get_id(), port_name);
    shared_ptr<ingress> ingr =
        shared_ptr<ingress>(new ingress(dst_id, src->get_id(), vq_ids,
                                        flits_per_queue, bw_to_xbar, rt,
                                        vc_alloc, pressures, log));
    egress_id src_id(src->get_id(), src_port_name);
    shared_ptr<egress> egr =
        shared_ptr<egress>(new egress(src_id, ingr, src->get_pressures(),
                                      link_bw, log));
    add_ingress(src->get_id(), ingr);
    src->add_egress(get_id(), egr);
}

void node::tick_positive_edge() throw(err) {
    for (ingresses_t::iterator n = ingresses.begin();
         n != ingresses.end(); ++n) {
        n->second->tick_positive_edge();
    }
    xbar.tick_positive_edge();
}

void node::tick_negative_edge() throw(err) {
    for (ingresses_t::iterator n = ingresses.begin();
         n != ingresses.end(); ++n) {
        n->second->tick_negative_edge();
    }
    xbar.tick_negative_edge();
    vc_alloc->allocate();
}

