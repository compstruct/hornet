// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CPU_HPP__
#define __CPU_HPP__

#include <iostream>
#include <sstream>
#include <iomanip>
#include <boost/shared_ptr.hpp>
#include "logger.hpp"
#include "reg.hpp"
#include "mem.hpp"
#include "bridge.hpp"
#include "pe.hpp"

using namespace std;
using namespace boost;

class cpu : public pe {
public:
    explicit cpu(const pe_id &id, shared_ptr<mem> ram,
                 uint32_t pc, uint32_t stack_pointer, logger &log) throw(err);
    virtual ~cpu() throw();
    virtual void connect(shared_ptr<bridge> net_bridge) throw();
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);

private:
    uint32_t get(const gpr &r) const throw();
    uint32_t get(const hwr &r) const throw(exc_reserved_hw_reg);
    void set(const gpr &r, uint32_t val) throw();
    void set_hi_lo(uint64_t val) throw();
    void execute() throw(err);

private:
    uint32_t time; // increments every cycle
    uint32_t pc;
    uint32_t gprs[32];
    uint64_t hi_lo;
    shared_ptr<mem> ram;
    shared_ptr<bridge> net;
    bool jump_active;
    bool interrupts_enabled;
    unsigned jump_time;
    uint32_t jump_target;
    ostringstream stdout_buffer;
    logger &log;
private:
    void syscall(uint32_t syscall_no) throw(err);
    void flush_stdout() throw();
    template<class V> V load(const uint32_t &addr) throw(err);
    template<class V> void store(const uint32_t &addr, const V &val) throw(err);
private:
    cpu(const cpu &); // not permitted
};

inline uint32_t cpu::get(const gpr &r) const throw() {
    return r.get_no() == 0 ? 0 : gprs[r.get_no()];
}

inline uint32_t cpu::get(const hwr &r) const throw(exc_reserved_hw_reg) {
    switch (r.get_no()) {
    case 0: return get_id().get_numeric_id();
    case 1: return 0;
    case 2: return time;
    case 3: return 1; // spec ambiguous on whether cc_res is 3 or 4
    case 4: return 1; // spec ambiguous on whether cc_res is 3 or 4
    case 30: return 0x52697375;
    case 31: return 0x52697375;
    default: throw exc_reserved_hw_reg(r.get_no());
    }
}

inline void cpu::set(const gpr &r, uint32_t v) throw() {
    if (r.get_no() != 0) {
        LOG(log,5) << "[cpu " << get_id() << "]     " << r << " <- "
            << hex << setfill('0') << setw(8) << v << endl;
        gprs[r.get_no()] = v;
    }
}

inline void cpu::set_hi_lo(uint64_t v) throw() {
    LOG(log,5) << "[cpu " << get_id() << "]     hi,lo <- "
        << hex << setfill('0') << setw(16) << v << endl;
    hi_lo = v;
}

template <class V>
inline V cpu::load(const uint32_t &addr) throw(err) {
    V result = ram->load<V>(addr);
    LOG(log,5) << "[cpu " << get_id() << "]     "
        << setfill('0') << setw(2 * sizeof(V)) << result << " <- mem["
        << hex << setfill('0') << setw(8) << addr << "]" << endl;
    return result;
}

template<class V>
inline void cpu::store(const uint32_t &addr, const V &val) throw(err) {
    LOG(log,5) << "[cpu " << get_id() << "]     mem["
        << hex << setfill('0') << setw(8) << addr << "] <- "
        << setfill('0') << setw(2 * sizeof(V)) << val << endl;
    ram->store(addr, val);
}

#endif // __CPU_HPP__

