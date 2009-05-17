// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <string>
#include <utility>
#include <set>
#include <cassert>
#include <boost/static_assert.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include "cstdint.hpp"
#include "endian.hpp"
#include "node.hpp"
#include "bridge.hpp"
#include "set_router.hpp"
#include "set_channel_alloc.hpp"
#include "set_bridge_channel_alloc.hpp"
#include "cpu.hpp"
#include "injector.hpp"
#include "event_parser.hpp"
#include "sys.hpp"

typedef enum {
    PE_CPU = 0,
    PE_INJECTOR = 1,
    NUM_PES
} pe_type_t;

static uint32_t read_word(shared_ptr<ifstream> in) throw(err) {
    uint32_t word = 0xdeadbeef;
    in->read((char *) &word, 4);
    if (in->bad()) throw err_bad_mem_img();
    word = endian(word);
    return word;
}

static double read_double(shared_ptr<ifstream> in) throw(err) {
    uint64_t word = 0xdeadbeefdeadbeefLL;
    in->read((char *) &word, 8);
    if (in->bad()) throw err_bad_mem_img();
    word = endian(word);
    BOOST_STATIC_ASSERT(sizeof(double) == 8);
    double d;
    memcpy(&d, &word, sizeof(double));
    return d;
}

static void read_mem(uint8_t *ptr, uint32_t num_bytes,
                     shared_ptr<ifstream> in) throw(err) {
    assert(ptr);
    in->read((char *) ptr, num_bytes);
    if (in->bad()) throw err_bad_mem_img();
}

sys::sys(shared_ptr<ifstream> img, uint64_t stats_start,
         shared_ptr<vector<string> > events_files, logger &new_log) throw(err)
    : pes(), bridges(), nodes(), time(0),
      stats(new statistics(time, stats_start)), log(new_log) {
    uint32_t num_nodes = read_word(img);
    LOG(log,2) << "creating system with " << num_nodes << " node"
        << (num_nodes == 1 ? "" : "s") << "..." << endl;
    typedef map<unsigned, shared_ptr<router> > routers_t;
    routers_t node_rts;
    typedef map<unsigned, shared_ptr<channel_alloc> > vcas_t;
    typedef map<unsigned, shared_ptr<bridge_channel_alloc> > br_vcas_t;
    vcas_t n_vcas;
    br_vcas_t br_vcas;
    typedef event_parser::injectors_t injectors_t;
    typedef event_parser::flow_starts_t flow_starts_t;
    shared_ptr<injectors_t> injectors(new injectors_t());
    shared_ptr<flow_starts_t> flow_starts(new flow_starts_t());
    for (unsigned i = 0; i < num_nodes; ++i) {
        uint32_t id = read_word(img);
        shared_ptr<router> n_rt(new set_router(id, log));
        shared_ptr<channel_alloc>
            n_vca(new set_channel_alloc(id, log));
        shared_ptr<bridge_channel_alloc>
            b_vca(new set_bridge_channel_alloc(id, log));
        uint32_t flits_per_q = read_word(img);
        shared_ptr<node> n(new node(node_id(id), flits_per_q, n_rt, n_vca,
                                    log));
        uint32_t n2b_bw = read_word(img);
        uint32_t b2n_bw = read_word(img);
        uint32_t b2n_xbar_bw = read_word(img);
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
                                        flits_per_q, b2n_xbar_bw, stats, log));
        shared_ptr<set_bridge_channel_alloc> vca =
            static_pointer_cast<set_bridge_channel_alloc>(b_vca);
        shared_ptr<ingress> b_n_i = n->get_ingress_from(b->get_id());
        const ingress::queues_t &b_n_i_qs = b_n_i->get_queues();
        for (ingress::queues_t::const_iterator qi = b_n_i_qs.begin();
                 qi != b_n_i_qs.end(); ++qi) {
                vca->add_queue(qi->second);
        }
        uint32_t pe_type_word = read_word(img);
        if (pe_type_word >= NUM_PES) throw err_bad_mem_img();
        pe_type_t pe_type = static_cast<pe_type_t>(pe_type_word);
        shared_ptr<pe> p;
        switch (pe_type) {
        case PE_CPU: {
            uint32_t mem_start = read_word(img);
            uint32_t mem_size = read_word(img);
            shared_ptr<mem> m(new mem(id, mem_start, mem_size, log));
            read_mem(m->ptr(mem_start), mem_size, img);
            
            uint32_t cpu_entry_point = read_word(img);
            uint32_t cpu_stack_pointer = read_word(img);
            p = shared_ptr<pe>(new cpu(pe_id(id), m, cpu_entry_point,
                                       cpu_stack_pointer, log));
            break;
        }
        case PE_INJECTOR: {
            shared_ptr<injector> inj(new injector(id, time, log));
            p = inj;
            (*injectors)[id] = inj;
            break;
        }
        default:
            throw err_bad_mem_img();
        }
        p->connect(b);
        pes[id] = p;
        bridges[id] = b;
        nodes[id] = n;
        br_vcas[id] = b_vca;
        node_rts[id] = n_rt;
        n_vcas[id] = n_vca;
    }
    arbitration_t arb_scheme = static_cast<arbitration_t>(read_word(img));
    unsigned arb_min_bw = 0;
    unsigned arb_period = 0;
    unsigned arb_delay = 0;
    if (arb_scheme != AS_NONE) {
        arb_min_bw = read_word(img);
        arb_period = read_word(img);
        arb_delay = read_word(img);
    }
    uint32_t num_cxns = read_word(img);
    typedef set<tuple<uint32_t, uint32_t> > cxns_t;
    cxns_t cxns;
    LOG(log,2) << "network fabric has " << dec << num_cxns
               << " one-way link" << (num_cxns == 1 ? "" : "s") << endl;
    if (arb_scheme == AS_NONE) {
        LOG(log,2) << "    (no bidirectional arbitration)" << endl;
    } else {
        LOG(log,2) << "    (bidirectional arbitration every "
                   << dec << arb_period
                   << " cycle" << (arb_period == 1 ? "" : "s");
        if (arb_min_bw == 0) {
            LOG(log,2) << " with no minimum bandwidth" << endl;
        } else {
            LOG(log,2) << " with minimum bandwidth "
                       << dec << arb_min_bw << endl;
        }
    }
    for (unsigned i  = 0; i < num_cxns; ++i) {
        uint32_t link_src_n = read_word(img);
        char link_src_port_name = static_cast<char>(read_word(img));
        uint32_t link_dst_n = read_word(img);
        char link_dst_port_name = static_cast<char>(read_word(img));
        uint32_t link_bw = read_word(img);
        uint32_t to_xbar_bw = read_word(img);
        uint32_t link_num_queues = read_word(img);
        set<virtual_queue_id> queues;
        for (unsigned q = 0; q < link_num_queues; ++q) {
            queues.insert(virtual_queue_id(read_word(img)));
        }
        if (nodes.find(link_src_n) == nodes.end()) throw err_bad_mem_img();
        if (nodes.find(link_dst_n) == nodes.end()) throw err_bad_mem_img();
        nodes[link_dst_n]->connect_from(string(1, link_dst_port_name),
                                        nodes[link_src_n],
                                        string(1, link_src_port_name),
                                        queues, link_bw, to_xbar_bw);
        cxns.insert(make_tuple(link_src_n, link_dst_n));
    }
    if (arb_scheme != AS_NONE) {
        for (cxns_t::const_iterator i = cxns.begin(); i != cxns.end(); ++i) {
            uint32_t from, to;
            tie(from, to) = *i;
            if (from <= to
                && cxns.count(make_tuple(to, from)) > 0) {
                arbiters[*i] =
                    shared_ptr<arbiter>(new arbiter(time, nodes[from],
                                                    nodes[to], arb_scheme,
                                                    arb_min_bw, arb_period,
                                                    arb_delay, stats, log));
            }
        }
    }

    uint32_t num_routes = read_word(img);
    LOG(log,2) << "network contains " << dec << num_routes
        << " routing " << (num_routes == 1 ? "entry" : "entries") << endl;
    for (unsigned i = 0; i < num_routes; ++i) {
        uint32_t flow = read_word(img);
        uint32_t cur_n = read_word(img);
        uint32_t prev_n = read_word(img);
        if (prev_n == 0xffffffffUL) { // program the bridge
            uint32_t num_queues = read_word(img); // 0 is valid (= all queues)
            vector<tuple<virtual_queue_id, double> > qs;
            for (uint32_t q = 0; q < num_queues; ++q) {
                uint32_t vqid = read_word(img);
                double prop = read_double(img);
                if (prop <= 0) throw err_bad_mem_img();
                qs.push_back(make_tuple(virtual_queue_id(vqid), prop));
            }
            (*flow_starts)[flow] = cur_n;
            shared_ptr<set_bridge_channel_alloc> vca =
                static_pointer_cast<set_bridge_channel_alloc>(br_vcas[cur_n]);
            vca->add_route(flow_id(flow), qs);
        } else { // program a routing node
            uint32_t num_nodes = read_word(img);
            vector<tuple<node_id,double> > next_nodes;
            if (num_nodes == 0) throw err_bad_mem_img();
            for (uint32_t n = 0; n < num_nodes; ++n) {
                uint32_t next_n = read_word(img);
                double nprop = read_double(img);
                if (nprop <= 0) throw err_bad_mem_img();
                next_nodes.push_back(make_tuple(next_n, nprop));
                uint32_t num_queues = read_word(img); // 0 is valid
                vector<tuple<virtual_queue_id,double> > next_qs;
                for (uint32_t q = 0; q < num_queues; ++q) {
                    uint32_t qid = read_word(img);
                    double qprop = read_double(img);
                    if (qprop <= 0) throw err_bad_mem_img();
                    next_qs.push_back(make_tuple(qid, qprop));
                }
                shared_ptr<set_channel_alloc> vca =
                    static_pointer_cast<set_channel_alloc>(n_vcas[cur_n]);
                vca->add_route(prev_n, flow_id(flow), next_n, next_qs);
            }
            shared_ptr<set_router> r =
                static_pointer_cast<set_router>(node_rts[cur_n]);
            r->add_route(prev_n, flow_id(flow), next_nodes);
        }
    }
    event_parser ep(events_files, injectors, flow_starts);
    LOG(log,1) << "system created" << endl;
}

shared_ptr<statistics> sys::get_statistics() throw() { return stats; }

void sys::tick_positive_edge() throw(err) {
    LOG(log,1) << "[system] tick " << dec << time << endl;
    for (arbiters_t::iterator i = arbiters.begin(); i != arbiters.end(); ++i) {
        i->second->tick_positive_edge();
    }
    for (pes_t::iterator i = pes.begin(); i != pes.end(); ++i) {
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
    for (pes_t::iterator i = pes.begin(); i != pes.end(); ++i) {
        i->second->tick_negative_edge();
    }
    for (nodes_t::iterator i = nodes.begin(); i != nodes.end(); ++i) {
        i->second->tick_negative_edge();
    }
    for (bridges_t::iterator i = bridges.begin(); i != bridges.end(); ++i) {
        i->second->tick_negative_edge();
    }
}
