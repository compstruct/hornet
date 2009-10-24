// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MAIN_MEM_HPP__
#define __MAIN_MEM_HPP__

#include "mem.hpp"

class main_mem : public mem {
public:
    explicit main_mem(uint32_t id, uint32_t start, uint32_t size,
                      logger &log) throw(err);
    virtual ~main_mem() throw();
    virtual shared_ptr<mem_ifc> new_interface() throw();
    const uint8_t *ptr(const uint32_t &addr) const throw(err);
    uint8_t *ptr(const uint32_t &addr) throw(err);
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
    pe_id get_id() const throw();
private:
    const pe_id id;
    uint32_t start;
    uint32_t size;
    uint8_t *contents;
    typedef vector<shared_ptr<mem_ifc> > interfaces_t;
    interfaces_t interfaces;
    logger &log;
private:
    main_mem(const mem &); // not implemented
};

inline uint8_t *main_mem::ptr(const uint32_t &addr) throw(err) {
    if (addr < start || addr + 4 > start + size) {
        throw exc_bus_err(get_id().get_numeric_id(), addr, start, size);
    }
    return contents + addr - start;
}

inline const uint8_t *main_mem::ptr(const uint32_t &addr) const throw(err) {
    if (addr < start || addr + 4 > start + size) {
        throw exc_bus_err(get_id().get_numeric_id(), addr, start, size);
    }
    return contents + addr - start;
}

inline pe_id main_mem::get_id() const throw() { return id; }

#endif // __MAIN_MEM_HPP__

