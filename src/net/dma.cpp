// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "endian.hpp"
#include "dma.hpp"

ostream &operator<<(ostream &out, const dma_channel_id &id) {
    return out << hex << setfill('0') << setw(2) << id.id;
}

ostream &operator<<(ostream &out, const tuple<node_id, dma_channel_id> &id) {
    return out << id.get<0>() << ":" << id.get<1>();
}

dma_channel::dma_channel(node_id n_id, dma_channel_id d_id, uint32_t new_bw,
                         shared_ptr<virtual_queue> q,
                         shared_ptr<tile_statistics> new_stats,
                         logger &l) throw()
    : id(make_tuple(n_id, d_id)), bandwidth(new_bw), vq(q), 
      started(false), flow(), remaining_flits(0),
      mem(NULL), stats(new_stats), log(l) { }

dma_channel::~dma_channel() { }

ingress_dma_channel::
ingress_dma_channel(node_id n_id, dma_channel_id d_id, unsigned new_bw,
                    shared_ptr<virtual_queue> q,
                    shared_ptr<tile_statistics> s, logger &l) throw()
    : dma_channel(n_id, d_id, new_bw, q, s, l), pid_p(NULL) { }

ingress_dma_channel::~ingress_dma_channel() { }

void ingress_dma_channel::receive(void *dst, packet_id *pid,
                                  uint32_t len) throw(err) {
    assert(!busy());
    started = false;
    mem = dst;
    remaining_flits = len;
    pid_p = pid;
    if (pid_p) *pid_p = 0xffffffffffffffffULL;
}

bool ingress_dma_channel::has_waiting_flow() const throw() {
    return (!vq->front_is_empty() && vq->front_node_id().is_valid()
            && vq->front_vq_id().is_valid());
}

uint32_t ingress_dma_channel::get_flow_length() const throw(err) {
    return vq->front_num_remaining_flits_in_packet();
}

flow_id ingress_dma_channel::get_flow_id() const throw(err) {
    return vq->front_new_flow_id();
}

void ingress_dma_channel::tick_positive_edge() throw(err) {
    if (!vq->front_is_empty() && vq->front_is_head_flit()) {
        if (!vq->front_node_id().is_valid()) {
            assert(!vq->front_vq_id().is_valid());
            assert(vq->front_old_flow_id().is_valid());
            assert(!vq->front_new_flow_id().is_valid());
            vq->front_set_next_hop(vq->get_id().get<0>(),
                                   vq->front_old_flow_id());
        }
        if (!vq->front_vq_id().is_valid()) {
            vq->front_set_vq_id(vq->get_id().get<1>());
        }
    }
    unsigned i;
    for (i = 0; ((bandwidth == 0 || i < bandwidth)
                 && remaining_flits > 0 && !vq->front_is_empty()); ++i) {
        assert(vq->front_node_id().is_valid());
        assert(vq->front_vq_id().is_valid());
        if (vq->front_is_head_flit()) {
            flow = vq->front_new_flow_id();
            if (started) {
                throw exc_new_flow_mid_dma(flow.get_numeric_id(),
                                           id.get<0>().get_numeric_id(),
                                           id.get<1>().get_numeric_id());
            }
            const flit f = vq->front_flit();
            const head_flit &h = reinterpret_cast<const head_flit &>(f);
            if (h.get_count_in_stats()) {
                stats->receive_packet(h);
            }
        } else {
            if (mem) *((uint64_t *) mem) = endian(vq->front_flit().get_data());
            mem = mem ? (uint8_t *) mem + sizeof(uint64_t) : mem;
            --remaining_flits;
        }
        if (pid_p) *pid_p = vq->front_flit().get_packet_id();
        if (vq->front_flit().get_count_in_stats()) {
            stats->receive_flit(flow, vq->front_flit());
        }
        stats->vq_rd(vq->get_id(),vq->get_ingress_id());
        vq->front_pop();
        started = true;
    }
    if (i > 0) {
        LOG(log,3) << "[dma " << id << "] received "
                   << dec << i << " flit" << (i == 1 ? "" : "s") << endl;
    }
}

void ingress_dma_channel::tick_negative_edge() throw(err) { }

egress_dma_channel::
egress_dma_channel(node_id n_id, dma_channel_id d_id, unsigned new_bw,
                   shared_ptr<virtual_queue> q,
                   shared_ptr<bridge_channel_alloc> vca,
                   shared_ptr<tile_statistics> s, logger &l) throw()
    : dma_channel(n_id, d_id, new_bw, q, s, l), pid(0), vc_alloc(vca) { }

egress_dma_channel::~egress_dma_channel() { }

void egress_dma_channel::send(flow_id f, void *src, uint32_t len,
                              const packet_id &new_pid,
                              bool c_in_stats) throw(err) {
    assert(!busy());
    started = false;
    flow = f;
    mem = src;
    remaining_flits = len;
    pid = new_pid;
    count_in_stats = c_in_stats;
    vc_alloc->claim(vq->get_id());
}

void egress_dma_channel::tick_positive_edge() throw(err) {
    unsigned i;
    for (i = 0; ((bandwidth == 0 || i < bandwidth)
                 && remaining_flits > 0 && !vq->back_is_full()
                 && (started || !vq->back_is_mid_packet())); ++i) {
        if (!started) {
            head_flit f(flow, remaining_flits, pid, count_in_stats);
            vq->back_push(f);
            if (count_in_stats) {
                stats->send_packet(flow, pid, remaining_flits);
                stats->send_flit(flow, f);
                stats->vq_wr(vq->get_id(), vq->get_ingress_id());
            }
        } else {
            --remaining_flits;
            uint64_t v = (mem ? *((uint64_t *) mem)
                          : ((((uint64_t) flow.get_numeric_id()) << 32)
                             | remaining_flits));
            flit f(endian(v), pid, count_in_stats);
            vq->back_push(f);
            if (count_in_stats) {
                stats->send_flit(flow, f);
                stats->vq_wr(vq->get_id(), vq->get_ingress_id());
            }
            mem = mem ? (uint8_t *) mem + sizeof(uint64_t) : mem;
            if (remaining_flits == 0) vc_alloc->release(vq->get_id());
        }
        started = true;
    }
    if (i > 0) {
        LOG(log,3) << "[dma " << id << "] sent "
            << dec << i << " flit" << (i == 1 ? "" : "s") << endl;
    }
}
                
void egress_dma_channel::tick_negative_edge() throw(err) { }
