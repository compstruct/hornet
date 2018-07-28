// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <algorithm>
#include <iterator>
#include <climits>
#include "endian.hpp"
#include "node.hpp"
#include "bridge.hpp"
#include "channel_alloc.hpp"

class bogus_channel_alloc : public channel_alloc {
public:
    bogus_channel_alloc(node_id id, bool one_q_per_f, bool one_f_per_q,
                        logger &log);
    virtual ~bogus_channel_alloc();
    virtual void add_egress(std::shared_ptr<egress> egress);
    virtual void allocate();
};

bogus_channel_alloc::bogus_channel_alloc(node_id i, bool one_q_per_f,
                                         bool one_f_per_q, logger &l)
    : channel_alloc(i, one_q_per_f, one_f_per_q, l) { }

bogus_channel_alloc::~bogus_channel_alloc() { }

void bogus_channel_alloc::add_egress(std::shared_ptr<egress> egress) {
    abort();
}

void bogus_channel_alloc::allocate() {
    abort();
}

bridge::bridge(std::shared_ptr<node> n,
               std::shared_ptr<bridge_channel_alloc> bridge_vca,
               const set<virtual_queue_id> &n2b_vq_ids, unsigned n2b_bw,
               const set<virtual_queue_id> &b2n_vq_ids, unsigned b2n_bw,
               unsigned flits_per_queue, unsigned b2n_bw_to_xbar,
               bool one_q_per_f, bool one_f_per_q,
               std::shared_ptr<id_factory<packet_id> > pif,
               std::shared_ptr<tile_statistics> st,
               std::shared_ptr<vcd_writer> new_vcd,
               logger &l)
    : id(n->get_id()), target(n), vc_alloc(bridge_vca), incoming(), outgoing(),
      ingress_dmas(), egress_dmas(), vqids(), packet_id_factory(pif),
      stats(st), vcd(new_vcd), log(l) {
    unsigned dma_no = 1;
    if (n2b_vq_ids.size() > 32)
        throw err_too_many_bridge_queues(id.get_numeric_id(),
                                         n2b_vq_ids.size());
    LOG(log,3) << "bridge " << id << " created" << endl;

    // bridge -> node: bridge egress and node ingress
    ingress_id node_ingress_id(n->get_id(), "B");
    std::shared_ptr<ingress> node_ingress =
        std::shared_ptr<ingress>(new ingress(node_ingress_id, n->get_id(),
                                        b2n_vq_ids, flits_per_queue,
                                        b2n_bw_to_xbar,
                                        target->get_channel_alloc(),
                                        target->get_pressures(), stats,
                                        vcd, log));
    n->add_ingress(n->get_id(), node_ingress);
    std::shared_ptr<pressure_tracker> br_pt(new pressure_tracker(id, log));
    egress_id bridge_egress_id(n->get_id(), "X");
    outgoing = std::shared_ptr<egress>(new egress(bridge_egress_id, node_ingress,
                                             br_pt, b2n_bw, log));
    ingress::queues_t node_ingress_queues = node_ingress->get_queues();
    for (ingress::queues_t::const_iterator i = node_ingress_queues.begin();
         i != node_ingress_queues.end(); ++i, ++dma_no) {
        egress_dma_channel *p =
            new egress_dma_channel(id, dma_no, b2n_bw, i->second, vc_alloc,
                                   st, log);
        std::shared_ptr<egress_dma_channel> d = std::shared_ptr<egress_dma_channel>(p);
        egress_dmas[i->first] = d;
    }

    // node -> bridge
    std::shared_ptr<channel_alloc> br_vca(new bogus_channel_alloc(id, one_q_per_f,
                                                             one_f_per_q, log));
    copy(n2b_vq_ids.begin(), n2b_vq_ids.end(),
         back_insert_iterator<vector<virtual_queue_id> >(vqids));
    ingress_id bridge_ingress_id(n->get_id(), "X");
    incoming = std::shared_ptr<ingress>(new ingress(bridge_ingress_id, n->get_id(),
                                               n2b_vq_ids, flits_per_queue,
                                               UINT_MAX, br_vca, br_pt,
                                               stats, vcd, log));
    for (ingress::queues_t::const_iterator q = incoming->get_queues().begin();
         q != incoming->get_queues().end(); ++q) {
        n->add_queue_id(q->first);
    }
    egress_id node_egress_id(n->get_id(), "B");
    std::shared_ptr<egress> node_egress =
        std::shared_ptr<egress>(new egress(node_egress_id, incoming,
                                      target->get_pressures(), n2b_bw, log));
    n->add_egress(id, node_egress);
    ingress::queues_t bridge_ingress_queues = incoming->get_queues();
    for (ingress::queues_t::const_iterator i = bridge_ingress_queues.begin();
         i != bridge_ingress_queues.end(); ++i, ++dma_no) {
        ingress_dma_channel *p =
            new ingress_dma_channel(id, dma_no, n2b_bw, i->second, st, log);
        std::shared_ptr<ingress_dma_channel> d = std::shared_ptr<ingress_dma_channel>(p);
        ingress_dmas[i->first] = d;
    }
}

uint32_t bridge::get_transmission_done(uint32_t xmit_id) {
    if (xmit_id == 0)
        throw exc_bad_transmission(id.get_numeric_id(), xmit_id);
    virtual_queue_id vq_id(xmit_id - 1);
    uint32_t result;
    ingress_dmas_t::iterator idi;
    egress_dmas_t::iterator edi;
    if ((idi = ingress_dmas.find(vq_id)) != ingress_dmas.end()) {
        result = !idi->second->busy();
    } else if ((edi = egress_dmas.find(vq_id)) != egress_dmas.end()) {
        result = !edi->second->busy();
    } else {
        throw exc_bad_transmission(id.get_numeric_id(), xmit_id);
    }
    LOG(log,3) << "[bridge " << id << "] reporting transmission "
               << hex << setfill('0') << setw(2) << xmit_id << " as "
               << (result ? "done" : "not done") << endl;
    return result;
}

uint32_t bridge::get_waiting_queues() {
    uint32_t waiting = 0;
    assert(vqids.size() <= 32);
    for (unsigned i = 0; i < vqids.size(); ++i) {
        virtual_queue_id vqid = vqids[i];
        assert(ingress_dmas.find(vqid) != ingress_dmas.end());
        waiting |= (ingress_dmas[vqid]->has_waiting_flow() ? 1 : 0) << i;
    }
    LOG(log,3) << "[bridge " << id << "] reporting waiting queues as "
               << hex << setfill('0') << setw(8) << waiting << endl;
    return waiting;
}

uint32_t bridge::get_queue_flow_id(uint32_t queue) {
    assert(vqids.size() <= 32);
    if (queue >= vqids.size())
        throw exc_bad_queue(id.get_numeric_id(), queue);
    virtual_queue_id vq_id = vqids[queue];
    assert(ingress_dmas.find(vq_id) != ingress_dmas.end());
    flow_id flow = ingress_dmas[vq_id]->get_flow_id();
    LOG(log,3) << "[bridge " << id
               << "] reporting head packet flow on queue "
               << virtual_queue_id(queue) << " as " << flow << endl;
    return flow.get_numeric_id();
}

uint32_t bridge::get_queue_length(uint32_t queue) {
    assert(vqids.size() <= 32);
    if (queue >= vqids.size())
        throw exc_bad_queue(id.get_numeric_id(), queue);
    virtual_queue_id vq_id = vqids[queue];
    assert(ingress_dmas.find(vq_id) != ingress_dmas.end());
    uint32_t len = ingress_dmas[vq_id]->get_flow_length();
    LOG(log,3) << "[bridge " << id
               << "] reporting head packet length on queue "
               << virtual_queue_id(queue) << " as "
               << dec << len << " flits" << endl;
    return len;
}

uint32_t bridge::send(uint32_t flow, void *src, uint32_t len,
                      bool count_in_stats) {
    return send(flow, src, len, packet_id_factory->get_fresh_id(),
                count_in_stats);
}

uint32_t bridge::send(uint32_t flow, void *src, uint32_t len,
                      packet_id pid, bool count_in_stats) {
    LOG(log,3) << "[bridge " << id << "] sending " << dec << len
               << " flits on flow " << flow_id(flow);
    virtual_queue_id q = vc_alloc->request(flow);
    if (!q.is_valid()) {
        LOG(log,3) << ": blocked (channel busy)" << endl;
        return 0;
    }
    egress_dmas_t::iterator di = egress_dmas.find(q);
    assert(di != egress_dmas.end());
    assert(!di->second->busy()); // otherwise VC alloc should have failed
    uint32_t xmit_id = q.get_numeric_id() + 1;
    LOG(log,3) << ": started (transmission ID " << dec << xmit_id << ")"
               << endl;
    di->second->send(flow, src, len, pid, count_in_stats);
    return xmit_id;
}

uint32_t bridge::receive(void *dst, uint32_t queue, uint32_t len,
                         packet_id *pid_p) {
    assert(vqids.size() <= 32);
    if (queue >= vqids.size())
        throw exc_bad_queue(id.get_numeric_id(), queue);
    LOG(log,3) << "[bridge " << id << "] receiving " << dec << len
               << " flits from queue " << virtual_queue_id(queue);
    virtual_queue_id vq_id = vqids[queue];
    assert(ingress_dmas.find(vq_id) != ingress_dmas.end());
    std::shared_ptr<ingress_dma_channel> dma = ingress_dmas[vq_id];
    if (dma->busy()) {
        LOG(log,3) << ": blocked (channel busy)" << endl;
        return 0;
    }
    uint32_t xmit_id = vq_id.get_numeric_id() + 1;
    LOG(log,3) << ": started (transmission ID " << dec << xmit_id << ")"
               << endl;
    ingress_dmas[vq_id]->receive(dst, pid_p, len);
    return xmit_id;
}

void bridge::tick_positive_edge() {
    for (ingress_dmas_t::iterator i = ingress_dmas.begin();
         i != ingress_dmas.end(); ++i) {
        i->second->tick_positive_edge();
    }
    incoming->tick_positive_edge();
    for (egress_dmas_t::iterator i = egress_dmas.begin();
         i != egress_dmas.end(); ++i) {
        i->second->tick_positive_edge();
    }
}

void bridge::tick_negative_edge() {
    for (ingress_dmas_t::iterator i = ingress_dmas.begin();
         i != ingress_dmas.end(); ++i) {
        i->second->tick_negative_edge();
    }
    incoming->tick_negative_edge();
    for (egress_dmas_t::iterator i = egress_dmas.begin();
         i != egress_dmas.end(); ++i) {
        i->second->tick_negative_edge();
    }
}

bool bridge::is_drained() const {
    bool drained = true;
    for (ingress_dmas_t::const_iterator i = ingress_dmas.begin();
         i != ingress_dmas.end(); ++i) {
        drained &= !i->second->busy();
    }
    for (egress_dmas_t::const_iterator i = egress_dmas.begin();
         i != egress_dmas.end(); ++i) {
        drained &= !i->second->busy();
    }
    drained &= incoming->is_drained();
    return drained;
}

std::shared_ptr<egress> bridge::get_egress() { return outgoing; }

std::shared_ptr<ingress> bridge::get_ingress() { return incoming; }

std::shared_ptr<vector<uint32_t> > bridge::get_ingress_queue_ids() {
        std::shared_ptr<vector<uint32_t> > v(new vector<uint32_t>(vqids.size()));
    for (uint32_t i = 0; i < vqids.size(); ++i) v->push_back(i);
    return v;
}
