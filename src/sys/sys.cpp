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
#include "cpu.hpp"
#include "sys.hpp"

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

sys::sys(shared_ptr<ifstream> img, shared_ptr<logger> new_log) throw(err)
    : cpus(), bridges(), nodes(), time(0), log(new_log) {
    uint32_t num_nodes = read_word(img);
    log << verbosity(2) << "creating system with " << num_nodes << " node"
        << (num_nodes == 1 ? "" : "s") << "..." << endl;
    for (unsigned i = 0; i < num_nodes; ++i) {
        uint32_t id = read_word(img);
        uint32_t node_mem_size = read_word(img);
        shared_ptr<node> n(new node(node_id(id), node_mem_size, log));

        uint32_t bridge_dmas = read_word(img);
        uint32_t bridge_dma_bw = read_word(img);
        shared_ptr<bridge> b(new bridge(node_id(id), bridge_dmas,
                                        bridge_dma_bw, log));

        uint32_t bridge_num_queues = read_word(img);
        vector<virtual_queue_id> bridge_queues;
        for (uint32_t q = 0; q < bridge_num_queues; ++q) {
            bridge_queues.push_back(virtual_queue_id(read_word(img)));
        }
        n->connect(b, bridge_queues);
        b->connect(n);

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
    }
    arbitration_t arb_scheme = static_cast<arbitration_t>(read_word(img));
    uint32_t num_cxns = read_word(img);
    typedef set<pair<uint32_t, uint32_t> > cxns_t;
    cxns_t cxns;
    log << verbosity(2) << "network fabric has " << dec << num_cxns
        << " one-way link" << (num_cxns == 1 ? "" : "s")
        << " (" << (arb_scheme == AS_NONE ? "no " : "with ")
        << "bidirectional arbitration)" << endl;
    for (unsigned i  = 0; i < num_cxns; ++i) {
        uint32_t link_src_node = read_word(img);
        uint32_t link_dst_node = read_word(img);
        uint32_t link_bandwidth = read_word(img);
        uint32_t link_num_queues = read_word(img);
        vector<virtual_queue_id> queues;
        for (unsigned q = 0; q < link_num_queues; ++q) {
            queues.push_back(virtual_queue_id(read_word(img)));
        }
        nodes[link_src_node]->connect(nodes[link_dst_node], link_bandwidth,
                                      queues);
        cxns.insert(make_pair(link_src_node, link_dst_node));
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
    log << verbosity(2) << "network contains " << dec << num_routes
        << " flow" << (num_routes == 1 ? "" : "s") << endl;
    for (unsigned i = 0; i < num_routes; ++i) {
        uint32_t flow = read_word(img);
        uint32_t route_len = read_word(img);
        log << verbosity(2) << "flow " << flow_id(flow) << " with " << dec
            << route_len << " hop" << (route_len == 1 ? "" : "s") << endl;
        uint32_t cur_node;
        for (unsigned hop = 0; hop < route_len; ++hop) {
            uint32_t next_node = read_word(img);
            uint32_t next_queue = read_word(img);
            if (hop == 0) { // origin: program the bridge
                bridges[next_node]->add_route(flow_id(flow),
                                              virtual_queue_id(next_queue));
                cur_node = next_node;
            } else { // next hop: program the current node
                nodes[cur_node]->add_route(flow_id(flow), node_id(next_node),
                                           virtual_queue_id(next_queue));
                cur_node = next_node;
            }
        }
    }
    log << verbosity(2) << "system created" << endl;
}

void sys::tick_positive_edge() throw(err) {
    log << verbosity(1) << "[system] tick " << dec << time << endl;
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

