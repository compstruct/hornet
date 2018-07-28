// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MCPU_HPP__
#define __MCPU_HPP__

#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
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

class mcpu : public common_core {
public:
    explicit mcpu(  const pe_id                         &id, 
                    const uint64_t                      &time, 
                    uint32_t                            pc, 
                    uint32_t                            stack_pointer,
                    std::shared_ptr<id_factory<packet_id> >  pif,
                    std::shared_ptr<tile_statistics>         stats,
                    logger                              &log,
                    std::shared_ptr<random_gen>              r,
                    std::shared_ptr<memory>                  instruction_memory,
                    std::shared_ptr<memory>                  data_memory,
                    std::shared_ptr<dram>                    backingDRAM_data,
                    uint32_t                            msg_queue_size, 
                    uint32_t                            bytes_per_flit) 
                   ;
    virtual ~mcpu();

/* core interface ----------------------------------------------------------- */

    virtual void execute();
    virtual void update_from_memory_requests();
    virtual uint64_t next_pkt_time();
    virtual bool is_drained() const;

    virtual void tick_positive_edge_memory();
    virtual void tick_negative_edge_memory();

/* mips scalar -------------------------------------------------------------- */

private:
    uint32_t get(const gpr &r) const;
    uint32_t get_s(const fpr &r) const;
    uint64_t get_d(const fpr &r) const;        
    uint32_t get(const hwr &r) const;
    bool get_cp1_cf(const cfr &r) const;
    void set(const gpr &r, uint32_t val);
    void set_s(const fpr &r, uint32_t v);
    void set_d(const fpr &r, uint64_t v);
    void set_cp1_cf(const cfr &r, bool value);
    void set_hi_lo(uint64_t val);
    void execute(std::shared_ptr<instr> i);

private:
    bool running;
    uint64_t instr_count;
    bool enable_memory_hierarchy;
    int32_t byte_count; // for the MIPS ISA (= 4)    
    uint32_t pc;
    uint32_t gprs[32];
    uint32_t fprs[32];
    bool fpcfs[8]; // condition flags, coprocessor 1
    uint64_t hi_lo;
    std::shared_ptr<bridge> net;
    bool jump_active;
    bool interrupts_enabled;
    unsigned jump_time;
    uint32_t jump_target;
    ostringstream stdout_buffer;

private:
    void syscall(uint32_t syscall_no);
    void trap();
    void flush_stdout();

/* memory hierarchy --------------------------------------------------------- */

private: // instruction memory hierarchy
    std::shared_ptr<memory> i_memory;

private: // instruction/data interface
    
    std::shared_ptr<memoryRequest> pending_request_instruction;
    std::shared_ptr<memoryRequest> pending_request_data;

    uint32_t pending_request_byte_count;
    bool pending_request_read_gpr;
    bool pending_request_read_fpr;
    bool pending_request_memory_write;
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
    std::shared_ptr<instr> instruction_fetch_complete(uint32_t pc);

    // data {read,write}{fetch,complete} interface
    void data_complete();
    void data_complete_helper(shared_array<uint32_t>, uint32_t);
    inline bool pending_data_memory_request() { 
        return  pending_request_read_gpr | 
                pending_request_read_fpr | 
                pending_request_memory_write; 
    }
    inline void close_memory_op() {
        pending_request_data = std::shared_ptr<memoryRequest>(); // NULLIFY
        pending_request_read_gpr = false;
        pending_request_read_fpr = false;
        pending_request_memory_write = false;
    }
    void data_fetch_to_gpr( const gpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend);
    void data_fetch_to_fpr( const fpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes);
    void data_fetch_read(   const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend);
    void data_fetch_write(  const uint32_t &addr, 
                            const uint64_t &val, 
                            const uint32_t &bytes);

protected: // memory hierarchy processing & inherited members
    inline std::shared_ptr<memory> nearest_memory_instruction() {
        return i_memory;
    }
    inline std::shared_ptr<memory> nearest_memory_data() {
        return m_memory;
    }
    virtual maddr_t form_maddr(uint64_t);
    virtual uint32_t form_maddr_space(uint64_t addr);

/* utility  ----------------------------------------------------------------- */

private:
    const static int MAX_BUFFER_SIZE = 256; // used by syscalls

    std::shared_ptr<dram> backingDRAM;

    int fid_key;
    FILE * fid_value;
};

/* -------------------------------------------------------------------------- */
/* MIPS                                                                       */
/* -------------------------------------------------------------------------- */

inline uint32_t mcpu::get(const gpr &r) const {
    return r.get_no() == 0 ? 0 : gprs[r.get_no()];
}
inline uint32_t mcpu::get_s(const fpr &r) const {
    return fprs[r.get_no()];
}
inline uint64_t mcpu::get_d(const fpr &r) const {
    assert(r.get_no() % 2 == 0);
    uint64_t b = 0x00000000ffffffffULL & fprs[r.get_no()];
    uint64_t t = (((uint64_t) fprs[r.get_no()+1]) << 32);
    uint64_t ret = t | b;
    //if (get_id() == 0) 
    //    printf( "[mcpu %d] Got: %016llX from f[%d]&f[%d]\n",  get_id().get_numeric_id(), 
    //        (long long unsigned int) ret, r.get_no(), r.get_no()+1);
    return ret;
}
inline uint32_t mcpu::get(const hwr &r) const {
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
inline bool mcpu::get_cp1_cf(const cfr &r) const {
    return fpcfs[r.get_no()];
}

inline void mcpu::set(const gpr &r, uint32_t v) {
    if (r.get_no() != 0) {
        //cout << "[mcpu " << get_id() << "]     " << r << " <- "
        //    << hex << setfill('0') << setw(8) << v << endl;
        gprs[r.get_no()] = v;
    }
}
inline void mcpu::set_s(const fpr &r, uint32_t v) {
    LOG(log,5) << "[mcpu.fp " << get_id() << "]     " << r << " <- "
        << hex << setfill('0') << setw(8) << v << endl;
    fprs[r.get_no()] = v;
}
inline void mcpu::set_d(const fpr &r, uint64_t v) {
    assert(r.get_no() % 2 == 0);
    uint32_t b = (uint32_t) v;
    uint32_t t = (uint32_t) (v >> 32);
    fprs[r.get_no()] = b;
    fprs[r.get_no()+1] = t;
    //if (get_id() == 0) 
    //    printf( "[mcpu %d] Set: f[%d]=%x,f[%d]=%x\n",  get_id().get_numeric_id(), 
    //        r.get_no(), b, r.get_no()+1, t);
}
inline void mcpu::set_hi_lo(uint64_t v) {
    LOG(log,5) << "[mcpu " << get_id() << "]     hi,lo <- "
        << hex << setfill('0') << setw(16) << v << endl;
    hi_lo = v;
}
inline void mcpu::set_cp1_cf(const cfr &r, bool value) {
    fpcfs[r.get_no()] = value;
}

#endif // __MCPU_HPP__


