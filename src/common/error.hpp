// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ERROR_HPP__
#define __ERROR_HPP__

#include <string>
#include <iostream>
#include "cstdint.hpp"

using namespace std;

class err {
protected:
    err();
    virtual ~err();
    friend ostream &operator<<(ostream &, const err &);
private:
    virtual void show_to(ostream &out) const = 0;
};

class err_panic : public err {
public:
    explicit err_panic(const string &message);
    explicit err_panic(const char *message);
    virtual ~err_panic();
private:
    virtual void show_to(ostream &out) const;
protected:
    string msg;
};

class err_out_of_mem : public err_panic {
public:
    explicit err_out_of_mem();
    virtual ~err_out_of_mem();
};

class err_thread_spawn : public err_panic {
public:
    explicit err_thread_spawn(const string &msg);
    virtual ~err_thread_spawn();
};

class err_tbd : public err_panic {
public:
    explicit err_tbd(const string &message);
    explicit err_tbd(const char *message);
    virtual ~err_tbd();
private:
    virtual void show_to(ostream &out) const;
};

class err_bad_mem_img : public err {
public:
    explicit err_bad_mem_img();
    virtual ~err_bad_mem_img();
private:
    virtual void show_to(ostream &out) const;
};

class err_bad_reg : public err {
public:
    explicit err_bad_reg(const unsigned register_number);
    virtual ~err_bad_reg();
private:
    virtual void show_to(ostream &out) const;
private:
    const unsigned reg_no;
};

// an exception while executing user program
class err_runtime_exc : public err {
protected:
    err_runtime_exc();
    virtual ~err_runtime_exc();
private:
    virtual void show_to(ostream &out) const = 0;
};

class exc_bad_instr : public err_runtime_exc {
public:
    explicit exc_bad_instr(const uint32_t instr_encoding);
    virtual ~exc_bad_instr();
private:
    virtual void show_to(ostream &out) const;
    const uint32_t encoding;
};

class exc_reserved_hw_reg : public err {
public:
    explicit exc_reserved_hw_reg(const unsigned register_number);
    virtual ~exc_reserved_hw_reg();
private:
    virtual void show_to(ostream &out) const;
private:
    const unsigned reg_no;
};

class exc_bus_err : public err_runtime_exc {
public:
    explicit exc_bus_err(const uint32_t id, const uint32_t addr,
                         const uint32_t start, const uint32_t size);
    virtual ~exc_bus_err();
private:
    virtual void show_to(ostream &out) const;
private:
    const uint32_t id;
    const uint32_t addr;
    const uint32_t start;
    const uint32_t num_bytes;
};

class exc_int_overflow : public err_runtime_exc {
public:
    explicit exc_int_overflow();
    virtual ~exc_int_overflow();
private:
    virtual void show_to(ostream &out) const;
};

class exc_addr_align : public err_runtime_exc {
public:
    explicit exc_addr_align();
    virtual ~exc_addr_align();
private:
    virtual void show_to(ostream &out) const;
};

class exc_bad_syscall : public err_runtime_exc {
public:
    explicit exc_bad_syscall(uint32_t syscall_no);
    virtual ~exc_bad_syscall();
    const uint32_t syscall_no;
private:
    virtual void show_to(ostream &out) const;
};

class exc_syscall_exit : public err_runtime_exc {
public:
    explicit exc_syscall_exit(uint32_t exit_code);
    virtual ~exc_syscall_exit();
    const uint32_t exit_code;
private:
    virtual void show_to(ostream &out) const;
};

class exc_no_network : public err_runtime_exc {
public:
    explicit exc_no_network(uint32_t cpu);
    virtual ~exc_no_network();
private:
    virtual void show_to(ostream &out) const;
private:
    uint32_t cpu;
};

class exc_new_flow_mid_dma : public err_runtime_exc {
public:
    explicit exc_new_flow_mid_dma(uint32_t flow, uint32_t node,
                                  uint32_t dma);
    virtual ~exc_new_flow_mid_dma();
private:
    const uint32_t flow;
    const uint32_t node;
    const uint32_t dma;
    virtual void show_to(ostream &out) const;
};

class exc_bad_queue : public err_runtime_exc {
public:
    explicit exc_bad_queue(uint32_t node, uint32_t queue);
    virtual ~exc_bad_queue();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_empty_queue : public err {
public:
    explicit err_empty_queue(uint32_t node, uint32_t queue);
    virtual ~err_empty_queue();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_too_many_bridge_queues : public err {
public:
    explicit err_too_many_bridge_queues(uint32_t node, uint32_t num);
    virtual ~err_too_many_bridge_queues();
private:
    const uint32_t node;
    const uint32_t num;
    virtual void show_to(ostream &out) const;
};

class err_claimed_queue : public err {
public:
    explicit err_claimed_queue(uint32_t node, uint32_t queue);
    virtual ~err_claimed_queue();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};


class err_duplicate_queue : public err {
public:
    explicit err_duplicate_queue(uint32_t node, uint32_t queue);
    virtual ~err_duplicate_queue();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_link_queue : public err {
public:
    explicit err_duplicate_link_queue(uint32_t node, uint32_t target,
                                      uint32_t queue);
    virtual ~err_duplicate_link_queue();
private:
    const uint32_t node;
    const uint32_t target; // target node
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_bridge_queue : public err {
public:
    explicit err_duplicate_bridge_queue(uint32_t node, uint32_t queue);
    virtual ~err_duplicate_bridge_queue();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_ingress : public err {
public:
    explicit err_duplicate_ingress(uint32_t node, uint32_t src);
    virtual ~err_duplicate_ingress();
private:
    const uint32_t node;
    const uint32_t src;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_egress : public err {
public:
    explicit err_duplicate_egress(uint32_t node, uint32_t dst);
    virtual ~err_duplicate_egress();
private:
    const uint32_t node;
    const uint32_t dst;
    virtual void show_to(ostream &out) const;
};

class err_bad_next_hop : public err {
public:
    explicit err_bad_next_hop(uint32_t node, uint32_t flow,
                              uint32_t next_node);
    virtual ~err_bad_next_hop();
private:
    const uint32_t node;
    const uint32_t flow;
    const uint32_t next_node;
    virtual void show_to(ostream &out) const;
};

class err_bad_next_hop_queue : public err {
public:
    explicit err_bad_next_hop_queue(uint32_t node, uint32_t flow,
                                    uint32_t next_node, uint32_t next_queue)
       ;
    virtual ~err_bad_next_hop_queue();
private:
    const uint32_t node;
    const uint32_t flow;
    const uint32_t next_node;
    const uint32_t next_queue;
    virtual void show_to(ostream &out) const;
};

class err_bad_neighbor : public err {
public:
    explicit err_bad_neighbor(uint32_t node, uint32_t neighbor_node);
    virtual ~err_bad_neighbor();
private:
    const uint32_t node;
    const uint32_t neighbor;
    virtual void show_to(ostream &out) const;
};

class exc_bad_flow : public err_runtime_exc {
public:
    explicit exc_bad_flow(uint32_t node, uint32_t flow);
    virtual ~exc_bad_flow();
private:
    const uint32_t node;
    const uint32_t flow;
    virtual void show_to(ostream &out) const;
};

class exc_bad_flow_from : public err_runtime_exc {
public:
    explicit exc_bad_flow_from(uint32_t node, uint32_t src_node,
                               uint32_t flow);
    virtual ~exc_bad_flow_from();
private:
    const uint32_t node;
    const uint32_t src_node;
    const uint32_t flow;
    virtual void show_to(ostream &out) const;
};

class err_route_not_static : public err {
public:
    explicit err_route_not_static();
    virtual ~err_route_not_static();
private:
    virtual void show_to(ostream &out) const;
};

class err_route_not_terminated : public err {
public:
    explicit err_route_not_terminated(uint32_t flow, uint32_t node);
    virtual ~err_route_not_terminated();
private:
    const uint32_t flow;
    const uint32_t node;
private:
    virtual void show_to(ostream &out) const;
};

class err_duplicate_flow : public err {
public:
    explicit err_duplicate_flow(uint32_t node, uint32_t flow);
    virtual ~err_duplicate_flow();
private:
    const uint32_t node;
    const uint32_t flow;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_flow_rename : public err {
public:
    explicit err_duplicate_flow_rename(uint32_t to, uint32_t from1,
                                       uint32_t from2);
    virtual ~err_duplicate_flow_rename();
private:
    const uint32_t to;    // renamed flow (target)
    const uint32_t from1; // first original flow ID
    const uint32_t from2; // second original flow ID
    virtual void show_to(ostream &out) const;
};

class exc_bad_transmission : public err_runtime_exc {
public:
    explicit exc_bad_transmission(uint32_t node, uint32_t xmit_id);
    virtual ~exc_bad_transmission();
private:
    const uint32_t node;
    const uint32_t xmit_id;
    virtual void show_to(ostream &out) const;
};

class exc_dma_busy : public err_runtime_exc {
public:
    explicit exc_dma_busy(uint32_t node, uint32_t dma);
    virtual ~exc_dma_busy();
private:
    const uint32_t node;
    const uint32_t dma;
    virtual void show_to(ostream &out) const;
};

class err_bad_arb_scheme : public err {
public:
    explicit err_bad_arb_scheme(uint32_t scheme);
    virtual ~err_bad_arb_scheme();
private:
    const uint32_t arb_scheme;
    virtual void show_to(ostream &out) const;
};

class err_bad_arb_min_bw : public err {
public:
    explicit err_bad_arb_min_bw(uint32_t src, uint32_t dst, uint32_t s2d_bw,
                                uint32_t d2s_bw, uint32_t min_bw);
    virtual ~err_bad_arb_min_bw();
private:
    const uint32_t src;
    const uint32_t dst;
    const uint32_t s2d_bw;
    const uint32_t d2s_bw;
    const uint32_t min_bw;
    virtual void show_to(ostream &out) const;
};

class err_parse : public err {
public:
    err_parse(const string &file,
              const string &msg = string("parse error"));
    err_parse(const string &file, unsigned line,
              const string &msg = string("parse error"));
    virtual ~err_parse();
private:
    const string file;
    const unsigned line;
    const string msg;
    virtual void show_to(ostream &out) const;
};

class err_bad_shmem_cfg : public err {
public:
    explicit err_bad_shmem_cfg(const string &message);
    explicit err_bad_shmem_cfg(const char *message);
    virtual ~err_bad_shmem_cfg();
private:
    virtual void show_to(ostream &out) const;
protected:
    string msg;
};


ostream &operator<<(ostream &, const err &);

#endif // __ERROR_HPP__

