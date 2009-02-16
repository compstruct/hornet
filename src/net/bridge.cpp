// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "endian.hpp"
#include "node.hpp"
#include "bridge.hpp"

ostream &operator<<(ostream &out, const dma_channel_id &id) {
    return out << hex << setfill('0') << setw(2) << id.id;
}

dma_channel::dma_channel(node_id nid, dma_channel_id did, uint32_t new_bw,
                         shared_ptr<logger> new_log) throw()
    : id(pair<node_id, dma_channel_id>(nid, did)), bandwidth(new_bw),
      egress(true), started(false), flow(0xdeadbeef), remaining_flits(0),
      mem(NULL), vq(shared_ptr<virtual_queue>()), log(new_log) { }

void dma_channel::send(flow_id new_flow, shared_ptr<virtual_queue> q,
                       void *src, uint32_t len) throw(err) {
    if (busy()) throw exc_dma_busy(id.first.get_numeric_id(),
                                   id.second.get_numeric_id());
    egress = true;
    started = false;
    flow = new_flow;
    vq = q;
    mem = src;
    remaining_flits = len;
}

void dma_channel::receive(void *dst, shared_ptr<virtual_queue> src,
                          uint32_t len) throw(err) {
    if (busy()) throw exc_dma_busy(id.first.get_numeric_id(),
                                   id.second.get_numeric_id());
    egress = false;
    started = false;
    mem = dst;
    vq = src;
    remaining_flits = len;
}

void dma_channel::tick_positive_edge() throw(err) {
    if (egress) {
        for (unsigned i = 0;
             i < bandwidth && remaining_flits > 0 && !vq->full()
             && (started || vq->ingress_new_flow()); ++i) {
            if (!started) {
                vq->push(head_flit(flow, remaining_flits));
            } else {
                vq->push(flit(endian(*((uint64_t *) mem))));
                mem = (uint8_t *) mem + sizeof(uint64_t);
                --remaining_flits;
            }
            started = true;
        }
    } else { // ingress
        for (unsigned i = 0;
             i < bandwidth && remaining_flits > 0 && !vq->empty(); ++i) {
            if (vq->egress_new_flow()) {
                flow = vq->get_egress_flow_id();
                if (started) throw
                    exc_new_flow_mid_dma(flow.get_numeric_id(),
                                         id.first.get_numeric_id(),
                                         id.second.get_numeric_id());
            } else {
                *((uint64_t *) mem) = endian(vq->front().get_data());
                mem = (uint8_t *) mem + sizeof(uint64_t);
                --remaining_flits;
            }
            vq->pop();
            started = true;
        }
    }
}

void dma_channel::tick_negative_edge() throw(err) { }

bridge::bridge(node_id new_id, uint32_t num_dmas, uint32_t dma_bw,
               shared_ptr<logger> new_log) throw (err)
    : id(new_id), dmas(), vqs(), routes(), log(new_log) {
    for (unsigned i = 0; i < num_dmas; ++i)
        dmas.push_back(shared_ptr<dma_channel>(new dma_channel(id, i, dma_bw,
                                                               log)));
    log << verbosity(3) << "bridge " << id << " created with "
        << dec << num_dmas << " DMA channel" << (num_dmas == 1 ? "" : "s")
        << " with bandwidth " << dec << dma_bw << endl;
}

uint32_t bridge::get_transmission_done(uint32_t xmit_id) throw() {
    if (xmit_id == 0 || xmit_id > dmas.size())
        throw exc_bad_transmission(id.get_numeric_id(), xmit_id);
    uint32_t result = !dmas[xmit_id-1]->busy();
    log << verbosity(3) << "[bridge " << id << "] reporting transmission "
        << hex << setfill('0') << setw(2) << xmit_id << " as "
        << (result ? "done" : "not done") << endl;
    return result;
}

uint32_t bridge::get_waiting_queues() throw() {
    uint32_t waiting = 0;
    for (queues_t::const_iterator i = vqs.begin(); i != vqs.end(); ++i) {
        shared_ptr<virtual_queue> q = i->second;
        waiting |= (((!q->empty() && q->egress_new_flow()) ? 1 : 0)
                    << q->get_id().second.get_numeric_id());
    }
    log << verbosity(3) << "[bridge " << id << "] reporting waiting queues as "
        << hex << setfill('0') << setw(8) << waiting << endl;
    return waiting;
}

void bridge::tick_positive_edge() throw(err) {
    for (dmas_t::iterator i = dmas.begin(); i != dmas.end(); ++i)
        (*i)->tick_positive_edge();
}

void bridge::tick_negative_edge() throw(err) {
    for (dmas_t::iterator i = dmas.begin(); i != dmas.end(); ++i)
        (*i)->tick_negative_edge();
}

uint32_t bridge::send(uint32_t flow, void *src, uint32_t len) throw(err) {
    log << verbosity(3) << "[bridge " << id << "] sending " << len
        << " flits on flow " << flow_id(flow) << endl;
    routes_t::iterator ri = routes.find(flow);
    if (ri == routes.end())
        throw exc_bad_bridge_flow(id.get_numeric_id(), flow);
    dmas_t::iterator di;
    for (di = dmas.begin(); di != dmas.end(); ++di) if (!(*di)->busy()) break;
    if (di == dmas.end()) return 0;
    (*di)->send(flow_id(flow), ri->second, src, len);
    return (*di)->get_id().get_numeric_id() + 1;
}

uint32_t bridge::receive(void *dst, uint32_t queue, uint32_t len) throw(err) {
    log << verbosity(3) << "[bridge " << id << "] receiving " << len
        << " flits from queue " << virtual_queue_id(queue) << endl;
    queues_t::iterator vq_p = vqs.find(virtual_queue_id(queue));
    if (vq_p == vqs.end()) throw exc_bad_queue(id.get_numeric_id(), queue);
    dmas_t::iterator di;
    for (di = dmas.begin(); di != dmas.end(); ++di) if (!(*di)->busy()) break;
    if (di == dmas.end()) return 0;
    (*di)->receive(dst, vq_p->second, len);
    return (*di)->get_id().get_numeric_id() + 1;
}

void bridge::connect(shared_ptr<node> new_target) throw(err) {
    target = new_target;
    target->add_incoming_link(id);
    log << verbosity(3) << "bridge " << id
        << " linked to node " << target->get_id() << endl;
}

void bridge::add_queue(shared_ptr<virtual_queue> vq) throw(err) {
    vq->claim(id);
    if (vqs.find(vq->get_id().second) != vqs.end())
        throw err_duplicate_bridge_queue(id.get_numeric_id(),
                                         vq->get_id().second.get_numeric_id());
    vqs[vq->get_id().second] = vq;
    log << verbosity(3) << "bridge " << id
        << " owns queue " << vq->get_id() << endl;
}

void bridge::add_route(flow_id flow, virtual_queue_id qid) throw(err) {
    routes_t::const_iterator ri = routes.find(flow);
    if (ri != routes.end())
        throw err_duplicate_bridge_flow(id.get_numeric_id(),
                                        flow.get_numeric_id());
    routes[flow] = target->get_virtual_queue(qid);
    log << verbosity(3) << "bridge " << id << " routing flow " << flow
        << " to " << target->get_id() << ":" << qid << endl;
}

uint32_t bridge::get_queue_flow_id(uint32_t queue) throw(err) {
    log << verbosity(3) << "[bridge " << id
        << "] query for head packet flow on queue "
        << virtual_queue_id(queue) << endl;
    queues_t::const_iterator vq_p = vqs.find(virtual_queue_id(queue));
    if (vq_p == vqs.end()) throw exc_bad_queue(id.get_numeric_id(), queue);
    flow_id flow = vq_p->second->get_egress_flow_id();
    log << verbosity(3) << "[bridge " << id
        << "] reporting head packet flow on queue "
        << virtual_queue_id(queue) << " as " << flow << endl;
    return flow.get_numeric_id();
}

uint32_t bridge::get_queue_length(uint32_t queue) throw(err) {
    log << verbosity(3) << "[bridge " << id
        << "] query for head packet length on queue "
        << virtual_queue_id(queue) << endl;
    queues_t::const_iterator vq_p = vqs.find(virtual_queue_id(queue));
    if (vq_p == vqs.end()) throw exc_bad_queue(id.get_numeric_id(), queue);
    uint32_t len = vq_p->second->get_egress_flow_length();
    log << verbosity(3) << "[bridge " << id
        << "] reporting head packet length on queue "
        << virtual_queue_id(queue) << " as " << len << " flits" << endl;
    return len;
}

