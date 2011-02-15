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

/* Master TODO list
   
   Bugs:
    1.) When a double is stored or loaded from memory and crosses a cache line 
        boundary, an exception is thrown.  Right now, we set huge cache lines 
        to make it very unlikely that this will happen.  We should properly 
        handle this case in the future.
        ---> see if you can get the compiler to ensure that this doesn't happen
    2.) when __H_enable_memory_hierarchy is called, data collected by 
        __H_read_line is lost.  Fix: place the __H_read_line result in a 
        temporary buffer (dest) and move it over the final buffer manually (done 
        circa 2/11/11 in blackscholes.c) 
   TODO:
    1.) Get fopen to support more than one file, and 'w' modes. 
   Common Qs (?):
    1.) why does enable_memory hierarchy exist?
    Changes to benchmarks:
    1.) In structs, etc... change datatypes that are < 32b to 32b. (word    
        alignment---see OptionType in blackscholes.OptionData_
*/

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

/* mips scalar -------------------------------------------------------------- */

private:
    uint32_t get(const gpr &r) const throw();
    uint32_t get_s(const fpr &r) const throw();
    uint64_t get_d(const fpr &r) const throw();        
    uint32_t get(const hwr &r) const throw(exc_reserved_hw_reg);
    bool get_cp1_cf(const cfr &r) const throw();
    void set(const gpr &r, uint32_t val) throw();
    void set_s(const fpr &r, uint32_t v) throw();
    void set_d(const fpr &r, uint64_t v) throw();
    void set_cp1_cf(const cfr &r, bool value) throw();
    void set_hi_lo(uint64_t val) throw();
    void execute(shared_ptr<instr> i) throw(err);

private:
    bool running;
    uint64_t instr_count;
    int32_t byte_count; // for the MIPS ISA (= 4)    
    uint32_t pc;
    uint32_t gprs[32];
    uint32_t fprs[32];
    bool fpcfs[8]; // condition flags, coprocessor 1
    uint64_t hi_lo;
    shared_ptr<bridge> net;
    bool jump_active;
    bool interrupts_enabled;
    unsigned jump_time;
    uint32_t jump_target;
    ostringstream stdout_buffer;

private:
    void syscall(uint32_t syscall_no) throw(err);
    void trap() throw(err);
    void flush_stdout() throw();

/* memory hierarchy --------------------------------------------------------- */

private: // instruction memory hierarchy
    map<int, shared_ptr<memory> > m_i_memory_hierarchy;
    int m_i_max_memory_level;
    int m_i_min_memory_level;
    void add_to_i_memory_hierarchy(int level, shared_ptr<memory> mem);

private: // instruction/data interface

    // instruction
    mreq_id_t pending_instruction_reqid;
    bool pending_i_request;

    // data
    mreq_id_t pending_reqid;
    shared_ptr<memoryRequest> pending_req;
    bool pending_request_gpr;
    bool pending_request_fpr;
    bool pending_request_memory;
    bool pending_lw_sign_extend;
    gpr pending_lw_gpr;
    fpr pending_lw_fpr;

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
    void data_complete_helper(const shared_ptr<memoryRequest> &req);
    inline bool pending_request() { 
        return pending_request_gpr | pending_request_fpr | pending_request_memory; 
    }
    inline void close_memory_op() {
        pending_req = shared_ptr<memoryRequest>(); // NULL
        nearest_memory()->finish(pending_reqid);
        pending_request_gpr = false;
        pending_request_fpr = false;
        pending_request_memory = false;
    }
    void data_fetch_to_gpr( const gpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err);
    void data_fetch_to_fpr( const fpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes) throw(err);
    void data_fetch_read(   const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err);
    void data_fetch_write(  const uint32_t &addr, 
                            const uint64_t &val, 
                            const uint32_t &bytes) throw(err);

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
                                        shared_ptr<mem> memory_backing_store,
                                        shared_ptr<dram> dram_backing_store);

/* utility  ----------------------------------------------------------------- */

private:
    const static int MAX_BUFFER_SIZE = 256; // used by syscalls

    bool enable_memory_hierarchy; 
    
    int fid_key;
    FILE * fid_value;
    shared_ptr<dramController> backingDRAM_data;
};

/* -------------------------------------------------------------------------- */
/* MIPS                                                                       */
/* -------------------------------------------------------------------------- */

inline uint32_t mcpu::get(const gpr &r) const throw() {
    return r.get_no() == 0 ? 0 : gprs[r.get_no()];
}
inline uint32_t mcpu::get_s(const fpr &r) const throw() {
    return fprs[r.get_no()];
}
inline uint64_t mcpu::get_d(const fpr &r) const throw() {
    assert(r.get_no() % 2 == 0);
    uint64_t b = 0x00000000ffffffffULL & fprs[r.get_no()];
    uint64_t t = (((uint64_t) fprs[r.get_no()+1]) << 32);
    uint64_t ret = t | b;
    //if (get_id() == 0) 
    //    printf( "[mcpu %d] Got: %016llX from f[%d]&f[%d]\n",  get_id().get_numeric_id(), 
    //        (long long unsigned int) ret, r.get_no(), r.get_no()+1);
    return ret;
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
inline bool mcpu::get_cp1_cf(const cfr &r) const throw() {
    return fpcfs[r.get_no()];
}

inline void mcpu::set(const gpr &r, uint32_t v) throw() {
    if (r.get_no() != 0) {
        LOG(log,5) << "[mcpu " << get_id() << "]     " << r << " <- "
            << hex << setfill('0') << setw(8) << v << endl;
        gprs[r.get_no()] = v;
    }
}
inline void mcpu::set_s(const fpr &r, uint32_t v) throw() {
    LOG(log,5) << "[mcpu.fp " << get_id() << "]     " << r << " <- "
        << hex << setfill('0') << setw(8) << v << endl;
    fprs[r.get_no()] = v;
}
inline void mcpu::set_d(const fpr &r, uint64_t v) throw() {
    assert(r.get_no() % 2 == 0);
    uint32_t b = (uint32_t) v;
    uint32_t t = (uint32_t) (v >> 32);
    fprs[r.get_no()] = b;
    fprs[r.get_no()+1] = t;
    //if (get_id() == 0) 
    //    printf( "[mcpu %d] Set: f[%d]=%x,f[%d]=%x\n",  get_id().get_numeric_id(), 
    //        r.get_no(), b, r.get_no()+1, t);
}
inline void mcpu::set_hi_lo(uint64_t v) throw() {
    LOG(log,5) << "[mcpu " << get_id() << "]     hi,lo <- "
        << hex << setfill('0') << setw(16) << v << endl;
    hi_lo = v;
}
inline void mcpu::set_cp1_cf(const cfr &r, bool value) throw() {
    fpcfs[r.get_no()] = value;
}

#endif // __MCPU_HPP__

