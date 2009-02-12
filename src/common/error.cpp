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


exc_bus_err::exc_bus_err(const uint32_t new_addr, const uint32_t new_start,
                         const uint32_t new_size) throw()
    : addr(new_addr), start(new_start), num_bytes(new_size) { }
exc_bus_err::~exc_bus_err() throw() { }
void exc_bus_err::show_to(ostream &out) const {
    out << hex << setfill('0') << "bus error: address 0x"
        << setw(8) << addr
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

exc_empty_queue::exc_empty_queue(uint32_t new_node, uint32_t new_queue) throw()
    : node(new_node), queue(new_queue) { }
exc_empty_queue::~exc_empty_queue() throw() { }
void exc_empty_queue::show_to(ostream &out) const {
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

err_duplicate_link::err_duplicate_link(uint32_t new_node,
                                       uint32_t new_target) throw()
    : node(new_node), target(new_target) { }
err_duplicate_link::~err_duplicate_link() throw() { }
void err_duplicate_link::show_to(ostream &out) const {
    out << "node " << setfill('0') << hex << setw(2) << node
        << " already connected to node " << setw(2) << target;
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

exc_bad_link_flow::exc_bad_link_flow(uint32_t new_node, uint32_t new_target,
                                     uint32_t new_flow) throw()
    : node(new_node), target(new_target), flow(new_flow) { }
exc_bad_link_flow::~exc_bad_link_flow() throw() { }
void exc_bad_link_flow::show_to(ostream &out) const {
    out << "link " << setfill('0') << hex << setw(2) << node << "->"
        << setw(2) << target << " has no route for flow " << setw(8) << flow;
}

exc_bad_bridge_flow::exc_bad_bridge_flow(uint32_t new_node,
                                         uint32_t new_flow) throw()
    : node(new_node), flow(new_flow) { }
exc_bad_bridge_flow::~exc_bad_bridge_flow() throw() { }
void exc_bad_bridge_flow::show_to(ostream &out) const {
    out << "bridge " << setfill('0') << hex << setw(2) << node
        << " has no route for flow " << setw(8) << flow;
}


err_duplicate_flow::err_duplicate_flow(uint32_t new_node,
                                       uint32_t new_flow) throw()
    : node(new_node), flow(new_flow) { }
err_duplicate_flow::~err_duplicate_flow() throw() { }
void err_duplicate_flow::show_to(ostream &out) const {
    out << "node " << setfill('0') << hex << setw(2) << node
        << " already has a route for flow " << setw(2) << flow;
}

err_duplicate_bridge_flow::
err_duplicate_bridge_flow(uint32_t new_node, uint32_t new_flow) throw()
    : node(new_node), flow(new_flow) { }
err_duplicate_bridge_flow::~err_duplicate_bridge_flow() throw() { }
void err_duplicate_bridge_flow::show_to(ostream &out) const {
    out << "bridge " << setfill('0') << hex << setw(2) << node
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

