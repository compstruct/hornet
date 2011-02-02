// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MCPU_HPP__
#define __MCPU_HPP__

#include <iostream>
#include <sstream>
#include <iomanip>
#include <boost/shared_ptr.hpp>
#include "logger.hpp"
#include "reg.hpp"
#include "mem.hpp"
#include "bridge.hpp"
#include "pe.hpp"
#include "statistics.hpp"
#include "core.hpp"
#include "dramController.hpp"
#include "tile.hpp"
#include "cache.hpp"
#include "instr.hpp"

using namespace std;
using namespace boost;

class mcpu : public core {
public:
    explicit mcpu(  const pe_id                         &id, 
                    const uint64_t                      &time, 
                    uint32_t                            pc, 
                    uint32_t                            stack_pointer,
                    shared_ptr<id_factory<packet_id> >  pif,
                    shared_ptr<tile_statistics>         stats,
                    logger                              &log,
                    shared_ptr<random_gen>              r,
                    core_cfg_t                          core_cfgs) 
                    throw(err);
    virtual ~mcpu() throw();

/* core interface ----------------------------------------------------------- */

    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_drained() const throw();
    virtual void exec_core();

/* mips --------------------------------------------------------------------- */

private:
    uint32_t get(const gpr &r) const throw();
    uint32_t get(const hwr &r) const throw(exc_reserved_hw_reg);
    void set(const gpr &r, uint32_t val) throw();
    void set_hi_lo(uint64_t val) throw();
    void execute(shared_ptr<instr> i) throw(err);

private:
    bool running;
    uint64_t instr_count;
    int32_t byte_count; // for the MIPS ISA (= 4)    
    uint32_t pc;
    uint32_t gprs[32];
    uint64_t hi_lo;
    shared_ptr<bridge> net;
    bool jump_active;
    bool interrupts_enabled;
    unsigned jump_time;
    uint32_t jump_target;
    ostringstream stdout_buffer;

private:
    void syscall(uint32_t syscall_no) throw(err);
    void flush_stdout() throw();

/* memory hierarchy --------------------------------------------------------- */

private: // instruction memory hierarchy
    map<int, shared_ptr<memory> > m_i_memory_hierarchy;
    int m_i_max_memory_level;
    int m_i_min_memory_level;
    void add_to_i_memory_hierarchy(int level, shared_ptr<memory> mem);

private: // instruction/data interface

    /*  Terminology:
            fetch - send a MemoryRequest to the memory hierarchy
            complete -  check the MemoryRequest's status, and finish() if it has 
                        returned
            read - lw for MIPS
            write - sw for MIPS
    */

    // instruction fetch/complete interface
    shared_ptr<instr> instruction_fetch_complete(uint32_t pc);

    // data {read,write}{fetch,complete} interface
    void data_complete();
    uint32_t data_complete_read(const shared_ptr<memoryRequest> &req); // helper
    inline void close_memory_op(); // helper
    void data_fetch_read(   const gpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err);
    void data_fetch_write(  const uint32_t &addr, 
                            const uint32_t &val, 
                            const uint32_t &bytes) throw(err);

private: // instruction/data metadata

    // instruction
    mreq_id_t pending_instruction_reqid;    
    bool pending_i_request;

    // data
    mreq_id_t pending_reqid;
    bool pending_request;
    bool pending_lw_sign_extend;
    gpr pending_lw_gpr;

protected: // memory hierarchy processing & inherited members
    inline shared_ptr<memory> nearest_i_memory() {
        return m_i_memory_hierarchy[m_i_min_memory_level];
    }
    virtual void mh_initiate();
    virtual void mh_update();
    virtual void mh_process();

public: // called by sys.cpp
    void initialize_memory_hierarchy(   uint32_t id, shared_ptr<tile> tile,
                                        shared_ptr<ifstream> img, 
                                        bool icash,
                                        shared_ptr<remoteMemory> rm,
                                        uint32_t mem_start,
                                        uint32_t mem_size, 
                                        shared_ptr<mem> m);
};

/* -------------------------------------------------------------------------- */
/* MIPS helpers                                                               */
/* -------------------------------------------------------------------------- */

inline uint32_t mcpu::get(const gpr &r) const throw() {
    return r.get_no() == 0 ? 0 : gprs[r.get_no()];
}

inline uint32_t mcpu::get(const hwr &r) const throw(exc_reserved_hw_reg) {
    switch (r.get_no()) {
    case 0: return get_id().get_numeric_id();
    case 1: return 0;
    case 2: return system_time;
    case 3: return 1; // spec ambiguous on whether cc_res is 3 or 4
    case 4: return 1; // spec ambiguous on whether cc_res is 3 or 4
    case 30: return 0x52697375;
    case 31: return 0x52697375;
    default: throw exc_reserved_hw_reg(r.get_no());
    }
}

inline void mcpu::set(const gpr &r, uint32_t v) throw() {
    if (r.get_no() != 0) {
        LOG(log,5) << "[mcpu " << get_id() << "]     " << r << " <- "
            << hex << setfill('0') << setw(8) << v << endl;
        gprs[r.get_no()] = v;
    }
}

inline void mcpu::set_hi_lo(uint64_t v) throw() {
    LOG(log,5) << "[mcpu " << get_id() << "]     hi,lo <- "
        << hex << setfill('0') << setw(16) << v << endl;
    hi_lo = v;
}

#endif // __MCPU_HPP__

