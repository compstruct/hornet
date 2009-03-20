// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "endian.hpp"
#include "dma.hpp"

ostream &operator<<(ostream &out, const dma_channel_id &id) {
    return out << hex << setfill('0') << setw(2) << id.id;
}

dma_channel::dma_channel(node_id n_id, dma_channel_id d_id, uint32_t new_bw,
                         shared_ptr<virtual_queue> q,
                         shared_ptr<statistics> new_stats, logger &l) throw()
    : id(make_pair(n_id, d_id)), bandwidth(new_bw), vq(q), 
      started(false), flow(0xdeadbeef), remaining_flits(0),
      mem(NULL), stats(new_stats), log(l) { }

dma_channel::~dma_channel() { }

ingress_dma_channel::
ingress_dma_channel(node_id n_id, dma_channel_id d_id, unsigned new_bw,
                    shared_ptr<virtual_queue> q,
                    shared_ptr<statistics> s, logger &l) throw()
    : dma_channel(n_id, d_id, new_bw, q, s, l) { }

ingress_dma_channel::~ingress_dma_channel() { }

void ingress_dma_channel::receive(void *dst, uint32_t len) throw(err) {
    if (busy()) throw exc_dma_busy(id.first.get_numeric_id(),
                                   id.second.get_numeric_id());
    started = false;
    mem = dst;
    remaining_flits = len;
}

bool ingress_dma_channel::has_waiting_flow() const throw() {
    return  !vq->empty() && vq->egress_new_flow();
}

uint32_t ingress_dma_channel::get_flow_length() const throw(err) {
    return vq->get_egress_flow_length();
}

flow_id ingress_dma_channel::get_flow_id() const throw(err) {
    return vq->get_egress_flow_id();
}

void ingress_dma_channel::tick_positive_edge() throw(err) {
    for (unsigned i = 0; ((bandwidth == 0 || i < bandwidth)
                          && remaining_flits > 0 && vq->egress_ready()); ++i) {
        if (vq->egress_new_flow()) {
            flow = vq->get_egress_flow_id();
            if (started)
                throw exc_new_flow_mid_dma(flow.get_numeric_id(),
                                           id.first.get_numeric_id(),
                                           id.second.get_numeric_id());
        } else {
            *((uint64_t *) mem) = endian(vq->front().get_data());
            mem = (uint8_t *) mem + sizeof(uint64_t);
            --remaining_flits;
        }
        vq->pop();
        stats->receive_flit(flow);
        started = true;
    }
}

void ingress_dma_channel::tick_negative_edge() throw(err) { }

egress_dma_channel::
egress_dma_channel(node_id n_id, dma_channel_id d_id, unsigned new_bw,
                   shared_ptr<virtual_queue> q,
                   shared_ptr<channel_alloc> vca,
                   shared_ptr<statistics> s, logger &l) throw()
    : dma_channel(n_id, d_id, new_bw, q, s, l), vc_alloc(vca) { }

egress_dma_channel::~egress_dma_channel() { }

void egress_dma_channel::send(flow_id f, void *src, uint32_t len) throw(err) {
    if (busy()) throw exc_dma_busy(id.first.get_numeric_id(),
                                   id.second.get_numeric_id());
    started = false;
    flow = f;
    mem = src;
    remaining_flits = len;
}

void egress_dma_channel::tick_positive_edge() throw(err) {
    for (unsigned i = 0; ((bandwidth == 0 || i < bandwidth)
                          && remaining_flits > 0 && !vq->full()
                          && (started || vq->ingress_new_flow())); ++i) {
        if (!started) {
            vq->push(head_flit(flow, remaining_flits));
        } else {
            vq->push(flit(endian(*((uint64_t *) mem))));
            mem = (uint8_t *) mem + sizeof(uint64_t);
            --remaining_flits;
            if (remaining_flits == 0) vc_alloc->release(vq->get_id().second);
        }
        stats->send_flit(flow);
        started = true;
    }
}

void egress_dma_channel::tick_negative_edge() throw(err) { }

