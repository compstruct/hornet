// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEM_HPP__
#define __MEM_HPP__

#include <boost/shared_ptr.hpp>
#include "cstdint.hpp"
#include "error.hpp"
#include "logger.hpp"
#include "endian.hpp"

using namespace std;
using namespace boost;

class mem {
public:
    explicit mem(uint32_t id, uint32_t start, uint32_t size,
                 logger &log) throw(err);
    virtual ~mem() throw();
    template<class V> V load(const uint32_t &addr) throw(err);
    template<class V> void store(const uint32_t &addr, const V &val) throw(err);
    const uint8_t *ptr(const uint32_t &addr) const throw(err);
    uint8_t *ptr(const uint32_t &addr) throw(err);
private:
    uint32_t id;
    uint32_t start;
    uint32_t size;
    uint8_t *contents;
    logger &log;
private:
    mem(const mem &); // not implemented
};

template <class V>
inline V mem::load(const uint32_t &addr) throw(err) {
    return endian(*((V *) ptr(addr)));
}

template<class V>
inline void mem::store(const uint32_t &addr, const V &val) throw(err) {
    *((V *) ptr(addr)) = endian(val);
}

inline uint8_t *mem::ptr(const uint32_t &addr) throw(err) {
    if (addr < start || addr + 4 > start + size)
        throw exc_bus_err(id, addr, start, size);
    return contents + addr - start;
}

inline const uint8_t *mem::ptr(const uint32_t &addr) const throw(err) {
    if (addr < start || addr + 4 > start + size)
        throw exc_bus_err(id, addr, start, size);
    return contents + addr - start;
}

#endif // __MEM_HPP__

