// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ERROR_HPP__
#define __ERROR_HPP__

#include <string>
#include <iostream>
#include "cstdint.hpp"

using namespace std;

class err {
public:
    virtual ~err() throw();
protected:
    err() throw();
    friend ostream &operator<<(ostream &, const err &);
private:
    virtual void show_to(ostream &out) const = 0;
};

class err_panic : public err {
public:
    explicit err_panic(const string &message) throw();
    explicit err_panic(const char *message) throw();
    virtual ~err_panic() throw();
private:
    virtual void show_to(ostream &out) const;
protected:
    string msg;
};

class err_out_of_mem : public err_panic {
public:
    explicit err_out_of_mem() throw();
    virtual ~err_out_of_mem() throw();
};

class err_tbd : public err_panic {
public:
    explicit err_tbd(const string &message) throw();
    explicit err_tbd(const char *message) throw();
    virtual ~err_tbd() throw();
private:
    virtual void show_to(ostream &out) const;
};

class err_bad_mem_img : public err {
public:
    explicit err_bad_mem_img() throw();
    virtual ~err_bad_mem_img() throw();
private:
    virtual void show_to(ostream &out) const;
};

class err_bad_reg : public err {
public:
    explicit err_bad_reg(const unsigned register_number) throw();
    virtual ~err_bad_reg() throw();
private:
    virtual void show_to(ostream &out) const;
private:
    const unsigned reg_no;
};

// an exception while executing user program
class err_runtime_exc : public err {
public:
    virtual ~err_runtime_exc() throw();
protected:
    err_runtime_exc() throw();
private:
    virtual void show_to(ostream &out) const = 0;
};

class exc_bad_instr : public err_runtime_exc {
public:
    explicit exc_bad_instr(const uint32_t instr_encoding) throw();
    virtual ~exc_bad_instr() throw();
private:
    virtual void show_to(ostream &out) const;
    const uint32_t encoding;
};

class exc_reserved_hw_reg : public err {
public:
    explicit exc_reserved_hw_reg(const unsigned register_number) throw();
    virtual ~exc_reserved_hw_reg() throw();
private:
    virtual void show_to(ostream &out) const;
private:
    const unsigned reg_no;
};

class exc_bus_err : public err_runtime_exc {
public:
    explicit exc_bus_err(const uint32_t addr, const uint32_t mem_start,
                         const uint32_t mem_size) throw();
    virtual ~exc_bus_err() throw();
private:
    virtual void show_to(ostream &out) const;
    const uint32_t addr;
    const uint32_t start;
    const uint32_t num_bytes;
};

class exc_int_overflow : public err_runtime_exc {
public:
    explicit exc_int_overflow() throw();
    virtual ~exc_int_overflow() throw();
private:
    virtual void show_to(ostream &out) const;
};

class exc_addr_align : public err_runtime_exc {
public:
    explicit exc_addr_align() throw();
    virtual ~exc_addr_align() throw();
private:
    virtual void show_to(ostream &out) const;
};

class exc_bad_syscall : public err_runtime_exc {
public:
    explicit exc_bad_syscall(uint32_t syscall_no) throw();
    virtual ~exc_bad_syscall() throw();
    const uint32_t syscall_no;
private:
    virtual void show_to(ostream &out) const;
};

class exc_syscall_exit : public err_runtime_exc {
public:
    explicit exc_syscall_exit(uint32_t exit_code) throw();
    virtual ~exc_syscall_exit() throw();
    const uint32_t exit_code;
private:
    virtual void show_to(ostream &out) const;
};

class exc_new_flow_mid_dma : public err_runtime_exc {
public:
    explicit exc_new_flow_mid_dma(uint32_t flow, uint32_t node,
                                  uint32_t dma) throw();
    virtual ~exc_new_flow_mid_dma() throw();
private:
    const uint32_t flow;
    const uint32_t node;
    const uint32_t dma;
    virtual void show_to(ostream &out) const;
};

class exc_bad_queue : public err_runtime_exc {
public:
    explicit exc_bad_queue(uint32_t node, uint32_t queue) throw();
    virtual ~exc_bad_queue() throw();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class exc_empty_queue : public err_runtime_exc {
public:
    explicit exc_empty_queue(uint32_t node, uint32_t queue) throw();
    virtual ~exc_empty_queue() throw();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_claimed_queue : public err {
public:
    explicit err_claimed_queue(uint32_t node, uint32_t queue) throw();
    virtual ~err_claimed_queue() throw();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};


class err_duplicate_queue : public err {
public:
    explicit err_duplicate_queue(uint32_t node, uint32_t queue) throw();
    virtual ~err_duplicate_queue() throw();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_link_queue : public err {
public:
    explicit err_duplicate_link_queue(uint32_t node, uint32_t target,
                                      uint32_t queue) throw();
    virtual ~err_duplicate_link_queue() throw();
private:
    const uint32_t node;
    const uint32_t target; // target node
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_bridge_queue : public err {
public:
    explicit err_duplicate_bridge_queue(uint32_t node, uint32_t queue) throw();
    virtual ~err_duplicate_bridge_queue() throw();
private:
    const uint32_t node;
    const uint32_t queue;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_link : public err {
public:
    explicit err_duplicate_link(uint32_t node, uint32_t target) throw();
    virtual ~err_duplicate_link() throw();
private:
    const uint32_t node;
    const uint32_t target;
    virtual void show_to(ostream &out) const;
};

class err_bad_next_hop : public err {
public:
    explicit err_bad_next_hop(uint32_t node, uint32_t flow,
                              uint32_t next_node, uint32_t next_queue) throw();
    virtual ~err_bad_next_hop() throw();
private:
    const uint32_t node;
    const uint32_t flow;
    const uint32_t next_node;
    const uint32_t next_queue;
    virtual void show_to(ostream &out) const;
};

class err_bad_neighbor : public err {
public:
    explicit err_bad_neighbor(uint32_t node, uint32_t neighbor_node) throw();
    virtual ~err_bad_neighbor() throw();
private:
    const uint32_t node;
    const uint32_t neighbor;
    virtual void show_to(ostream &out) const;
};

class exc_bad_link_flow : public err_runtime_exc {
public:
    explicit exc_bad_link_flow(uint32_t node, uint32_t target,
                               uint32_t flow) throw();
    virtual ~exc_bad_link_flow() throw();
private:
    const uint32_t node;
    const uint32_t target;
    const uint32_t flow;
    virtual void show_to(ostream &out) const;
};

class exc_bad_bridge_flow : public err_runtime_exc {
public:
    explicit exc_bad_bridge_flow(uint32_t node, uint32_t flow) throw();
    virtual ~exc_bad_bridge_flow() throw();
private:
    const uint32_t node;
    const uint32_t flow;
    virtual void show_to(ostream &out) const;
};


class err_duplicate_flow : public err {
public:
    explicit err_duplicate_flow(uint32_t node, uint32_t flow) throw();
    virtual ~err_duplicate_flow() throw();
private:
    const uint32_t node;
    const uint32_t flow;
    virtual void show_to(ostream &out) const;
};

class err_duplicate_bridge_flow : public err {
public:
    explicit err_duplicate_bridge_flow(uint32_t node, uint32_t flow) throw();
    virtual ~err_duplicate_bridge_flow() throw();
private:
    const uint32_t node;
    const uint32_t flow;
    virtual void show_to(ostream &out) const;
};

class exc_bad_transmission : public err_runtime_exc {
public:
    explicit exc_bad_transmission(uint32_t node, uint32_t xmit_id) throw();
    virtual ~exc_bad_transmission() throw();
private:
    const uint32_t node;
    const uint32_t xmit_id;
    virtual void show_to(ostream &out) const;
};

class exc_dma_busy : public err_runtime_exc {
public:
    explicit exc_dma_busy(uint32_t node, uint32_t dma) throw();
    virtual ~exc_dma_busy() throw();
private:
    const uint32_t node;
    const uint32_t dma;
    virtual void show_to(ostream &out) const;
};

class err_bad_arb_scheme : public err {
public:
    explicit err_bad_arb_scheme(uint32_t scheme) throw();
    virtual ~err_bad_arb_scheme() throw();
private:
    const uint32_t arb_scheme;
    virtual void show_to(ostream &out) const;
};

ostream &operator<<(ostream &, const err &);

#endif // __ERROR_HPP__

