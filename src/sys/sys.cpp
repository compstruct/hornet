// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <string>
#include <utility>
#include <set>
#include <cassert>
#include "cstdint.hpp"
#include "endian.hpp"
#include "node.hpp"
#include "bridge.hpp"
#include "static_router.hpp"
#include "static_channel_alloc.hpp"
#include "dynamic_channel_alloc.hpp"
#include "cpu.hpp"
#include "sys.hpp"

typedef enum {
    VCA_TABLE = 0,
    VCA_ROUND_ROBIN = 1,
    NUM_VCAS
} vca_type_t;

ostream &operator<<(ostream &out, const vca_type_t &vat) {
    switch (vat) {
    case VCA_TABLE:
        return out << "table-driven";
    case VCA_ROUND_ROBIN:
        return out << "round-robin";
    default:
        abort();
    }
}

static uint32_t read_word(shared_ptr<ifstream> in) throw(err) {
    uint32_t word = 0xdeadbeef;
    in->read((char *) &word, 4);
    word = endian(word);
    if (in->bad()) throw err_bad_mem_img();
    return word;
}

static void read_mem(uint8_t *ptr, uint32_t num_bytes,
                     shared_ptr<ifstream> in) throw(err) {
    assert(ptr);
    in->read((char *) ptr, num_bytes);
    if (in->bad()) throw err_bad_mem_img();
}

sys::sys(shared_ptr<ifstream> img, uint64_t stats_start, logger &new_log)
    throw(err) : cpus(), bridges(), nodes(), time(0),
                 stats(new statistics(time, stats_start)), log(new_log) {
    uint32_t vca_type_word = read_word(img);
    if (vca_type_word >= NUM_VCAS) throw err_bad_mem_img();
    vca_type_t vca_type = static_cast<vca_type_t>(vca_type_word);
    uint32_t num_nodes = read_word(img);
    LOG(log,2) << "creating system with " << num_nodes << " node"
        << (num_nodes == 1 ? "" : "s") << "..." << endl;
    typedef map<unsigned, shared_ptr<router> > routers_t;
    routers_t node_rts;
    typedef map<unsigned, shared_ptr<channel_alloc> > vcas_t;
    vcas_t br_vcas;
    vcas_t n_vcas;
    for (unsigned i = 0; i < num_nodes; ++i) {
        uint32_t id = read_word(img);
        shared_ptr<router> n_rt(new static_router(id, log));
        shared_ptr<channel_alloc> n_vca;
        shared_ptr<channel_alloc> b_vca;
        switch (vca_type) {
        case VCA_TABLE:
            n_vca =
                shared_ptr<channel_alloc>(new static_channel_alloc(id, log));
            b_vca =
                shared_ptr<channel_alloc>(new static_channel_alloc(id, log));
            break;
        case VCA_ROUND_ROBIN:
            n_vca =
                shared_ptr<channel_alloc>(new dynamic_channel_alloc(id, log));
            b_vca =
                shared_ptr<channel_alloc>(new dynamic_channel_alloc(id, log));
            break;
        default:
            throw err_bad_mem_img();
        }
        uint32_t flits_per_q = read_word(img);
        shared_ptr<node> n(new node(node_id(id), flits_per_q, n_rt, n_vca,
                                    log));

        uint32_t n2b_bw = read_word(img);
        uint32_t b2n_bw = read_word(img);
        uint32_t b2n_num_queues = read_word(img);
        set<virtual_queue_id> b2n_queues;
        for (uint32_t q = 0; q < b2n_num_queues; ++q) {
            b2n_queues.insert(virtual_queue_id(read_word(img)));
        }
        uint32_t n2b_num_queues = read_word(img);
        set<virtual_queue_id> n2b_queues;
        for (uint32_t q = 0; q < n2b_num_queues; ++q) {
            n2b_queues.insert(virtual_queue_id(read_word(img)));
        }
        shared_ptr<bridge> b(new bridge(n, b_vca,
                                        n2b_queues, n2b_bw, b2n_queues, b2n_bw,
                                        flits_per_q, stats, log));
        if (vca_type == VCA_ROUND_ROBIN) {
            shared_ptr<dynamic_channel_alloc> d_b_vca =
                static_pointer_cast<dynamic_channel_alloc>(b_vca);
            d_b_vca->add_egress(node_id(id), b->get_egress());
            shared_ptr<dynamic_channel_alloc> d_n_vca =
                static_pointer_cast<dynamic_channel_alloc>(n_vca);
            d_n_vca->add_egress(node_id(id), n->get_egress_to(node_id(id)));
        }
        uint32_t mem_start = read_word(img);
        uint32_t mem_size = read_word(img);
        shared_ptr<mem> m(new mem(mem_start, mem_size, log));
        read_mem(m->ptr(mem_start), mem_size, img);

        uint32_t cpu_entry_point = read_word(img);
        uint32_t cpu_stack_pointer = read_word(img);
        shared_ptr<cpu> c(new cpu(cpu_id(id), m, cpu_entry_point,
                                  cpu_stack_pointer, log));
        c->connect(b);
        cpus[id] = c;
        bridges[id] = b;
        nodes[id] = n;
        br_vcas[id] = b_vca;
        node_rts[id] = n_rt;
        n_vcas[id] = n_vca;
    }
    arbitration_t arb_scheme = static_cast<arbitration_t>(read_word(img));
    uint32_t num_cxns = read_word(img);
    typedef set<pair<uint32_t, uint32_t> > cxns_t;
    cxns_t cxns;
    LOG(log,2) << "network uses " << vca_type
        << " virtual channel allocation" << endl;
    LOG(log,2) << "network fabric has " << dec << num_cxns
        << " one-way link" << (num_cxns == 1 ? "" : "s")
        << " (" << (arb_scheme == AS_NONE ? "no " : "with ")
        << "bidirectional arbitration)" << endl;
    for (unsigned i  = 0; i < num_cxns; ++i) {
        uint32_t link_src_n = read_word(img);
        char link_src_port_name = static_cast<char>(read_word(img));
        uint32_t link_dst_n = read_word(img);
        char link_dst_port_name = static_cast<char>(read_word(img));
        uint32_t link_bandwidth = read_word(img);
        uint32_t link_num_queues = read_word(img);
        set<virtual_queue_id> queues;
        for (unsigned q = 0; q < link_num_queues; ++q) {
            queues.insert(virtual_queue_id(read_word(img)));
        }
        nodes[link_dst_n]->connect_from(string(1, link_dst_port_name),
                                        nodes[link_src_n],
                                        string(1, link_src_port_name),
                                        queues, link_bandwidth);
        if (vca_type == VCA_ROUND_ROBIN) {
            shared_ptr<dynamic_channel_alloc> vca =
                static_pointer_cast<dynamic_channel_alloc>(n_vcas[link_src_n]);
            vca->add_egress(node_id(link_dst_n),
                            nodes[link_src_n]->get_egress_to(link_dst_n));
        }
        cxns.insert(make_pair(link_src_n, link_dst_n));
    }
    if (arb_scheme != AS_NONE) {
        for (cxns_t::const_iterator i = cxns.begin(); i != cxns.end(); ++i) {
            if (i->first <= i->second
                && cxns.count(make_pair(i->second, i->first)) > 0) {
                arbiters[*i] =
                    shared_ptr<arbiter>(new arbiter(nodes[i->first],
                                                    nodes[i->second],
                                                    arb_scheme, log));
            }
        }
    }

    uint32_t num_routes = read_word(img);
    LOG(log,2) << "network contains " << dec << num_routes
        << " flow" << (num_routes == 1 ? "" : "s") << endl;
    for (unsigned i = 0; i < num_routes; ++i) {
        uint32_t flow = read_word(img);
        uint32_t route_len = read_word(img);
        LOG(log,2) << "flow " << flow_id(flow) << " with " << dec
            << route_len << " hop" << (route_len == 1 ? "" : "s") << endl;
        uint32_t cur_n = 0xdeadbeef;
        for (unsigned hop = 0; hop < route_len; ++hop) {
            uint32_t next_n = read_word(img);
            uint32_t next_q = 0xdeadbeef;
            if (vca_type == VCA_TABLE) {
                next_q = read_word(img);
            }
            if (hop == 0) { // origin: program the bridge
                if (vca_type == VCA_TABLE) {
                    shared_ptr<static_channel_alloc> vca =
                        static_pointer_cast<static_channel_alloc>
                            (br_vcas[next_n]);
                    vca->add_route(next_n, flow_id(flow),
                                   virtual_queue_id(next_q));
                }
                cur_n = next_n;
            } else { // next hop: program the current node
                if ((vca_type == VCA_TABLE) && (hop == route_len - 1)
                    && (next_n != cur_n))
                    throw err_route_not_terminated(flow, next_n);
                shared_ptr<static_router> r =
                    static_pointer_cast<static_router>(node_rts[cur_n]);
                r->add_route(flow_id(flow), next_n); // next node hop
                if (vca_type == VCA_ROUND_ROBIN && hop == route_len - 1) {
                    // final node -> bridge hop, missing in dynamic routes
                    shared_ptr<static_router> r =
                        static_pointer_cast<static_router>(node_rts[next_n]);
                    r->add_route(flow_id(flow), next_n);
                }
                if (vca_type == VCA_TABLE) { // VCA alloc route
                    shared_ptr<static_channel_alloc> vca =
                        static_pointer_cast<static_channel_alloc>
                            (n_vcas[cur_n]);
                    vca->add_route(next_n, flow_id(flow),
                                   virtual_queue_id(next_q));
                }
                cur_n = next_n;
            }
        }
    }
    LOG(log,1) << "system created" << endl;
}

shared_ptr<statistics> sys::get_statistics() throw() { return stats; }

void sys::tick_positive_edge() throw(err) {
    LOG(log,1) << "[system] tick " << dec << time << endl;
    for (arbiters_t::iterator i = arbiters.begin(); i != arbiters.end(); ++i) {
        i->second->tick_positive_edge();
    }
    for (cpus_t::iterator i = cpus.begin(); i != cpus.end(); ++i) {
        i->second->tick_positive_edge();
    }
    for (nodes_t::iterator i = nodes.begin(); i != nodes.end(); ++i) {
        i->second->tick_positive_edge();
    }
    for (bridges_t::iterator i = bridges.begin(); i != bridges.end(); ++i) {
        i->second->tick_positive_edge();
    }
    ++time;
}

void sys::tick_negative_edge() throw(err) {
    for (arbiters_t::iterator i = arbiters.begin(); i != arbiters.end(); ++i) {
        i->second->tick_negative_edge();
    }
    for (cpus_t::iterator i = cpus.begin(); i != cpus.end(); ++i) {
        i->second->tick_negative_edge();
    }
    for (nodes_t::iterator i = nodes.begin(); i != nodes.end(); ++i) {
        i->second->tick_negative_edge();
    }
    for (bridges_t::iterator i = bridges.begin(); i != bridges.end(); ++i) {
        i->second->tick_negative_edge();
    }
}

