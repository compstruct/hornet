// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "error.hpp"

err::err() throw() { }
err::~err() throw() { }
ostream &operator<<(ostream &out, const err &e) {
    e.show_to(out);
    return out;
}

err_panic::err_panic(const string &new_msg) throw() : msg(new_msg) { }
err_panic::err_panic(const char *new_msg) throw() : msg(new_msg) { }
err_panic::~err_panic() throw() { }
void err_panic::show_to(ostream &out) const {
    out << "PANIC: " << msg;
}

err_tbd::err_tbd(const string &new_msg) throw() : err_panic(new_msg) { }
err_tbd::err_tbd(const char *new_msg) throw() : err_panic(new_msg) { }
err_tbd::~err_tbd() throw() { }
void err_tbd::show_to(ostream &out) const {
    out << "TBD: " << msg;
}

err_out_of_mem::err_out_of_mem() throw() : err_panic("out of memory") { }
err_out_of_mem::~err_out_of_mem() throw() { }

err_bad_mem_img::err_bad_mem_img() throw() { }
err_bad_mem_img::~err_bad_mem_img() throw() { }
void err_bad_mem_img::show_to(ostream &out) const {
    out << "failed to read memory image";
}

err_bad_reg::err_bad_reg(unsigned new_reg_no) throw() : reg_no(new_reg_no) { }
err_bad_reg::~err_bad_reg() throw() { }
void err_bad_reg::show_to(ostream &out) const {
    out << "bad register number: " << reg_no;
}

err_runtime_exc::err_runtime_exc() throw() { }
err_runtime_exc::~err_runtime_exc() throw() { }

exc_bad_instr::exc_bad_instr(const uint32_t new_encoding) throw()
    : encoding(new_encoding) { }
exc_bad_instr::~exc_bad_instr() throw() { }
void exc_bad_instr::show_to(ostream &out) const {
    out << "unknown instruction encoding: " << encoding;
}

exc_reserved_hw_reg::exc_reserved_hw_reg(unsigned new_reg_no) throw()
    : reg_no(new_reg_no) { }
exc_reserved_hw_reg::~exc_reserved_hw_reg() throw() { }
void exc_reserved_hw_reg::show_to(ostream &out) const {
    out << "reserved hardware register: " << reg_no;
}

exc_bus_err::exc_bus_err(const uint32_t new_id, const uint32_t new_addr,
                         const uint32_t new_start,
                         const uint32_t new_size) throw()
    : id(new_id), addr(new_addr), start(new_start), num_bytes(new_size) { }
exc_bus_err::~exc_bus_err() throw() { }
void exc_bus_err::show_to(ostream &out) const {
    out << hex << setfill('0') << "memory " << setw(2) << id
        << ": address 0x" << setw(8) << addr
        << " outside memory [" << setw(8) << start << ".."
        << setw(8) << (start + num_bytes - 1) << "]";
}

exc_int_overflow::exc_int_overflow() throw() { }
exc_int_overflow::~exc_int_overflow() throw() { }
void exc_int_overflow::show_to(ostream &out) const {
    out << "integer overflow";
}

exc_addr_align::exc_addr_align() throw() { }
exc_addr_align::~exc_addr_align() throw() { }
void exc_addr_align::show_to(ostream &out) const {
    out << "integer overflow";
}

exc_bad_syscall::exc_bad_syscall(uint32_t n) throw() : syscall_no(n) { }
exc_bad_syscall::~exc_bad_syscall() throw() { }
void exc_bad_syscall::show_to(ostream &out) const {
    out << "bad system call number: " << syscall_no;
}

exc_syscall_exit::exc_syscall_exit(uint32_t n) throw() : exit_code(n) { }
exc_syscall_exit::~exc_syscall_exit() throw() { }
void exc_syscall_exit::show_to(ostream &out) const {
    out << "exit(" << exit_code << ")";
}

exc_no_network::exc_no_network(uint32_t new_cpu) throw() : cpu(new_cpu) { }
exc_no_network::~exc_no_network() throw() { }
void exc_no_network::show_to(ostream &out) const {
    out << "CPU " << setfill('0') << hex << setw(2) << cpu
        << " not connected to a network";
}

exc_new_flow_mid_dma::exc_new_flow_mid_dma(uint32_t new_flow, uint32_t new_node,
                                           uint32_t new_dma) throw()
    : flow(new_flow), node(new_node), dma(new_dma) { }
exc_new_flow_mid_dma::~exc_new_flow_mid_dma() throw() { }
void exc_new_flow_mid_dma::show_to(ostream &out) const {
    out << "new flow " << setfill('0') << hex << setw(8) << flow
        << " in the middle of DMA transfer on channel "
        << setw(2) << node << ":" << setw(2) << dma;
}

exc_bad_queue::exc_bad_queue(uint32_t new_node, uint32_t new_queue) throw()
    : node(new_node), queue(new_queue) { }
exc_bad_queue::~exc_bad_queue() throw() { }
void exc_bad_queue::show_to(ostream &out) const {
    out << "queue " << setfill('0') << hex
        << setw(2) << node << ":" << setw(2) << queue << " does not exist";
}

err_empty_queue::err_empty_queue(uint32_t new_node, uint32_t new_queue) throw()
    : node(new_node), queue(new_queue) { }
err_empty_queue::~err_empty_queue() throw() { }
void err_empty_queue::show_to(ostream &out) const {
    out << "queue " << setfill('0') << hex
        << setw(2) << node << ":" << setw(2) << queue << " is empty";
}

err_duplicate_queue::err_duplicate_queue(uint32_t new_node,
                                         uint32_t new_queue) throw()
    : node(new_node), queue(new_queue) { }
err_duplicate_queue::~err_duplicate_queue() throw() { }
void err_duplicate_queue::show_to(ostream &out) const {
    out << "queue " << setfill('0') << hex
        << setw(2) << node << ":" << setw(2) << queue << " already defined";
}

err_too_many_bridge_queues::err_too_many_bridge_queues(uint32_t new_node,
                                                       uint32_t nqs) throw()
    : node(new_node), num(nqs) { }
err_too_many_bridge_queues::~err_too_many_bridge_queues() throw() { }
void err_too_many_bridge_queues::show_to(ostream &out) const {
    out << "bridge " << setfill('0') << hex << setw(2) << node
        << " has more than 32 queues (" << dec << num << ")";
}

err_claimed_queue::err_claimed_queue(uint32_t new_node,
                                     uint32_t new_queue) throw()
    : node(new_node), queue(new_queue) { }
err_claimed_queue::~err_claimed_queue() throw() { }
void err_claimed_queue::show_to(ostream &out) const {
    out << "queue " << setfill('0') << hex
        << setw(2) << node << ":" << setw(2) << queue << " already claimed";
}

err_duplicate_bridge_queue::
err_duplicate_bridge_queue(uint32_t new_node, uint32_t new_queue) throw()
    : node(new_node), queue(new_queue) { }
err_duplicate_bridge_queue::~err_duplicate_bridge_queue() throw() { }
void err_duplicate_bridge_queue::show_to(ostream &out) const {
    out << "bridge " << setfill('0') << hex << setw(2) << node
        << " already owns queue " << setw(2) << queue;
}

err_duplicate_link_queue::err_duplicate_link_queue(uint32_t new_node,
                                                   uint32_t new_target,
                                                   uint32_t new_queue) throw()
    : node(new_node), target(new_target), queue(new_queue) { }
err_duplicate_link_queue::~err_duplicate_link_queue() throw() { }
void err_duplicate_link_queue::show_to(ostream &out) const {
    out << "link " << setfill('0') << hex << setw(2) << node << "->"
        << setw(2) << target << " already owns queue " << setw(2) << queue;
}

err_duplicate_ingress::err_duplicate_ingress(uint32_t new_node,
                                             uint32_t new_src) throw()
    : node(new_node), src(new_src) { }
err_duplicate_ingress::~err_duplicate_ingress() throw() { }
void err_duplicate_ingress::show_to(ostream &out) const {
    out << "ingress " << setfill('0') << hex << setw(2) << src
        << "->" << setw(2) << node << " already exists";
}

err_duplicate_egress::err_duplicate_egress(uint32_t new_node,
                                           uint32_t new_dst) throw()
    : node(new_node), dst(new_dst) { }
err_duplicate_egress::~err_duplicate_egress() throw() { }
void err_duplicate_egress::show_to(ostream &out) const {
    out << "egress " << setfill('0') << hex << setw(2) << node
        << "->" << setw(2) << dst << " already exists";
}

err_bad_next_hop::err_bad_next_hop(uint32_t new_node, uint32_t new_flow,
                                   uint32_t new_next_node, uint32_t new_queue)
  throw() : node(new_node), flow(new_flow), next_node(new_next_node),
            next_queue(new_queue) { }
err_bad_next_hop::~err_bad_next_hop() throw() { }
void err_bad_next_hop::show_to(ostream &out) const {
    out << "node " << setfill('0') << hex << setw(2) << node
        << " cannot route flow " << setw(8) << flow << " to "
        << setw(2) << next_node << ":" << setw(2) << next_queue
        << ": no connection to node " << setw(2) << next_node;
}

err_bad_neighbor::err_bad_neighbor(uint32_t new_node, uint32_t new_nbr) throw()
  : node(new_node), neighbor(new_nbr) { }
err_bad_neighbor::~err_bad_neighbor() throw() { }
void err_bad_neighbor::show_to(ostream &out) const {
    out << "node " << setfill('0') << hex << setw(2) << node
        << " has no connection to node " << setw(2) << neighbor;
}

exc_bad_flow::exc_bad_flow(uint32_t new_node, uint32_t new_flow) throw()
    : node(new_node), flow(new_flow) { }
exc_bad_flow::~exc_bad_flow() throw() { }
void exc_bad_flow::show_to(ostream &out) const {
    out << "node " << setfill('0') << hex << setw(2) << node 
        << " has no route for flow " << setw(8) << flow;
}

exc_bad_flow_from::exc_bad_flow_from(uint32_t new_node, uint32_t new_src_node,
                                     uint32_t new_flow) throw()
    : node(new_node), src_node(new_src_node), flow(new_flow) { }
exc_bad_flow_from::~exc_bad_flow_from() throw() { }
void exc_bad_flow_from::show_to(ostream &out) const {
    out << "node " << setfill('0') << hex << setw(2) << node 
        << " has no route for flow " << setw(8) << flow
        << " from node " << setw(2) << src_node;
}

err_route_not_static::err_route_not_static() throw() { }
err_route_not_static::~err_route_not_static() throw() { }
void err_route_not_static::show_to(ostream &out) const {
    out << "attempting to add a route to a dynamically-routed system";
}

err_route_not_terminated::err_route_not_terminated(uint32_t f, uint32_t n)
    throw() : flow(f), node(n) { }
err_route_not_terminated::~err_route_not_terminated() throw() { }
void err_route_not_terminated::show_to(ostream &out) const {
    out << "flow " << hex << setfill('0') << setw(8) << flow
        << " ends at node " << setw(2) << node
        << " without specifying a bridge queue";
}

err_duplicate_flow::err_duplicate_flow(uint32_t new_node,
                                       uint32_t new_flow) throw()
    : node(new_node), flow(new_flow) { }
err_duplicate_flow::~err_duplicate_flow() throw() { }
void err_duplicate_flow::show_to(ostream &out) const {
    out << "node " << setfill('0') << hex << setw(2) << node
        << " already has a route for flow " << setw(2) << flow;
}

exc_bad_transmission::
exc_bad_transmission(uint32_t new_node, uint32_t new_xmit_id) throw()
    : node(new_node), xmit_id(new_xmit_id) { }
exc_bad_transmission::~exc_bad_transmission() throw() { }
void exc_bad_transmission::show_to(ostream &out) const {
    out << "transmission " << setfill('0') << hex << setw(2) << node
        << ":" << setw(2) << xmit_id << " does not exist";
}

exc_dma_busy::exc_dma_busy(uint32_t new_node, uint32_t new_dma) throw()
    : node(new_node), dma(new_dma) { }
exc_dma_busy::~exc_dma_busy() throw() { }
void exc_dma_busy::show_to(ostream &out) const {
    out << "DMA channel " << setfill('0') << hex
        << setw(2) << node << ":" << setw(2) << dma << " busy";
}

err_bad_arb_scheme::err_bad_arb_scheme(uint32_t new_arb_scheme) throw()
  : arb_scheme(new_arb_scheme) { }
err_bad_arb_scheme::~err_bad_arb_scheme() throw() { }
void err_bad_arb_scheme::show_to(ostream &out) const {
    out << "invalid arbitration scheme code: " << dec << arb_scheme;
}

err_bad_arb_min_bw::err_bad_arb_min_bw(uint32_t new_src, uint32_t new_dst,
                                       uint32_t new_s2d_bw, uint32_t new_d2s_bw,
                                       uint32_t new_min_bw) throw()
    : src(new_src), dst(new_dst), s2d_bw(new_s2d_bw), d2s_bw(new_d2s_bw),
      min_bw(new_min_bw) { }
err_bad_arb_min_bw::~err_bad_arb_min_bw() throw() { }
void err_bad_arb_min_bw::show_to(ostream &out) const {
    out << "arbiter " << hex << setfill('0') << setw(2) << src << "<->"
        << setw(2) << dst << " can't guarantee bandwidth " << dec << min_bw
        << " given ->" << s2d_bw << " and <-" << d2s_bw; 
}

err_parse::err_parse(const string &f, const string &m) throw()
    : file(f), line(0), msg(m) { }
err_parse::err_parse(const string &f, unsigned l, const string &m) throw()
    : file(f), line(l), msg(m) { }

err_parse::~err_parse() throw() { }
void err_parse::show_to(ostream &out) const {
    if (file == "" && line == 0) {
        out << "unknown position";
    } else if (file == "") {
        out << "line " << dec << noshowpos << line;
    } else if (line == 0) {
        out << file;
    } else {
        out << file << ":" << dec << noshowpos << line;
    }
    out << ": " << msg;
}
