// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "endian.hpp"
#include "bridge.hpp"
#include "node.hpp"

connection::connection(node_id new_parent_id, shared_ptr<node> tgt,
                       unsigned max_bw, shared_ptr<logger> nlog) throw()
    : parent_id(new_parent_id), bandwidth(max_bw), target(tgt), vqs(), routes(),
      log(nlog) {
    log << verbosity(3) << "link " << hex << setfill('0')
        << setw(2) << parent_id << "->" << setw(2) << target->get_id()
        << " with bandwidth " << dec << bandwidth << " created" << endl;
}

void connection::add_queue(shared_ptr<virtual_queue> vq) throw(err) {
    vq->claim(target->get_id());
    if (vqs.find(vq->get_id().second) != vqs.end())
        throw err_duplicate_link_queue(parent_id.get_numeric_id(),
                                       target->get_id().get_numeric_id(),
                                       vq->get_id().second.get_numeric_id());
    vqs[vq->get_id().second] = vq;
    log << verbosity(3) << "link " << parent_id << "->" << target->get_id()
        << " owns queue " << vq->get_id() << endl;
}

void connection::add_route(flow_id flow, virtual_queue_id nbr_qid) throw(err) {
    routes_t::const_iterator ri = routes.find(flow);
    if (ri != routes.end())
        throw err_duplicate_flow(parent_id.get_numeric_id(),
                                 flow.get_numeric_id());
    routes[flow] = target->get_virtual_queue(nbr_qid);
    log << verbosity(3) << "link " << parent_id << "->" << target->get_id()
        << " routing flow " << flow << " to "
        << target->get_id() << ":" << nbr_qid << endl;
}

void connection::tick_positive_edge() throw(err) {
    assert(!vqs.empty());
    unsigned free_slots = target->get_free_incoming_slots(parent_id);
    queues_t::iterator cur_q = vqs.begin();
    for (unsigned i = 0; i < bandwidth && free_slots > 0; ++i) {
        // crappy round-robin
        for (; cur_q != vqs.end() && cur_q->second->empty(); ++cur_q);
        if (cur_q == vqs.end()) cur_q = vqs.begin(); // wrap
        for (; cur_q != vqs.end() && cur_q->second->empty(); ++cur_q);
        if (cur_q == vqs.end()) break; // no more flits to send

        // data transfer
        shared_ptr<virtual_queue> src_q = cur_q->second;
        assert(!src_q->empty());
        flow_id flow = src_q->get_egress_flow_id();
        routes_t::iterator ri = routes.find(flow);
        if (ri == routes.end())
            throw exc_bad_link_flow(parent_id.get_numeric_id(),
                                    target->get_id().get_numeric_id(),
                                    flow.get_numeric_id());
        shared_ptr<virtual_queue> dst_q = ri->second;
        if (src_q->egress_new_flow() && !dst_q->ingress_new_flow()) {
            ++cur_q; continue;  // try someone else in the next bw slot
        }
        --free_slots;
        dst_q->push(src_q->front());
        src_q->pop();
    }
}

void connection::tick_negative_edge() throw(err) { }

node::node(node_id new_id, uint32_t memsz, shared_ptr<logger> nlog) throw()
    : id(new_id), alloc(shared_ptr<common_alloc>(new common_alloc(memsz >> 3))),
      vqs(), links(), num_incoming_links(0), stale_num_slots(0), log(nlog) {
    assert(alloc);
    stale_num_slots = alloc->free_slots();
    log << verbosity(3) << "node " << id << " created with " << dec << memsz
        << " byte" << (memsz == 1 ? "" : "s") << " of queue memory ("
        << dec << (memsz >> 3) << " flit" << ((memsz >> 3) == 1 ? "" : "s")
        << ")" << endl;
}

void node::connect(shared_ptr<node> target, unsigned bw,
                   vector<virtual_queue_id> queues) throw(err) {
    shared_ptr<connection> cxn(new connection(id, target, bw, log));
    for (vector<virtual_queue_id>::const_iterator i = queues.begin();
         i != queues.end(); ++i) {
        if (vqs.find(*i) != vqs.end())
            throw err_duplicate_queue(get_id().get_numeric_id(),
                                      i->get_numeric_id());
        shared_ptr<virtual_queue> q(new virtual_queue(get_id(), *i, alloc,
                                                      log));
        vqs[q->get_id().second] = q;
        cxn->add_queue(q);
    }
    if (links.find(target->get_id()) != links.end())
        throw err_duplicate_link(get_id().get_numeric_id(),
                                 target->get_id().get_numeric_id());
    links[target->get_id()] = cxn;
    target->add_incoming_link(id);
}

void node::connect(shared_ptr<bridge> target,
                   vector<virtual_queue_id> queues) throw(err) {
    for (vector<virtual_queue_id>::const_iterator i = queues.begin();
         i != queues.end(); ++i) {
        if (vqs.find(*i) != vqs.end())
            throw err_duplicate_queue(get_id().get_numeric_id(),
                                      i->get_numeric_id());
        shared_ptr<virtual_queue> q(new virtual_queue(get_id(), *i, alloc,
                                                      log));
        vqs[q->get_id().second] = q;
        target->add_queue(q);
    }
}

void node::add_incoming_link(const node_id &from) throw(err) {
    ++num_incoming_links;
}

void node::add_route(flow_id flow, node_id neighbor_id,
                     virtual_queue_id neighbor_queue_id) throw(err) {
    if (links.find(neighbor_id) == links.end())
        throw err_bad_next_hop(get_id().get_numeric_id(),
                               flow.get_numeric_id(),
                               neighbor_id.get_numeric_id(),
                               neighbor_queue_id.get_numeric_id());
    links[neighbor_id]->add_route(flow, neighbor_queue_id);
}

void node::tick_positive_edge() throw(err) {
    for (queues_t::iterator n = vqs.begin(); n != vqs.end(); ++n) {
        n->second->tick_positive_edge();
    }
    for (links_t::iterator n = links.begin(); n != links.end(); ++n) {
        n->second->tick_positive_edge();
    }
}

void node::tick_negative_edge() throw(err) {
    for (links_t::iterator n = links.begin(); n != links.end(); ++n) {
        n->second->tick_negative_edge();
    }
    for (queues_t::iterator n = vqs.begin(); n != vqs.end(); ++n) {
        n->second->tick_negative_edge();
    }
    stale_num_slots = alloc->free_slots();
}

const shared_ptr<virtual_queue> node::get_virtual_queue(virtual_queue_id id)
    throw(err) {
    queues_t::iterator vq_p = vqs.find(id);
    if (vq_p == vqs.end())
        throw exc_bad_queue(get_id().get_numeric_id(), id.get_numeric_id());
    return vq_p->second;
}

