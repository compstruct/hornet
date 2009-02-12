// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ENDIAN_HPP__
#define __ENDIAN_HPP__

#include "config.hpp"
#include "cstdint.hpp"

inline uint32_t endian(uint32_t word) {
#ifdef WORDS_BIGENDIAN
    return word;
#else
    return (((word & 0xff) << 24) |
            ((word & 0xff00) << 8) |
            ((word >> 8) & 0xff00) |
            ((word >> 24) & 0xff));
#endif
}

inline int32_t endian(int32_t word) {
    return (int32_t) endian((uint32_t) word);
}

inline uint64_t endian(uint64_t word64) {
#ifdef WORDS_BIGENDIAN
    return dword;
#else
    return ((((uint64_t) endian((uint32_t) word64)) << 32)
            | endian((uint32_t) (word64 >> 32)));
#endif
}

inline int64_t endian(int64_t word64) {
    return (int64_t) endian((uint64_t) word64);
}

inline uint16_t endian(uint16_t halfword) {
#ifdef WORDS_BIGENDIAN
    return halfword;
#else
    return (((halfword & 0xff) << 8) | ((halfword & 0xff00) >> 8));
#endif
}

inline int16_t endian(int16_t word) {
    return (int16_t) endian((uint16_t) word);
}

inline uint8_t endian(uint8_t byte) { return byte; }

inline int8_t endian(int8_t byte) { return byte; }

#endif // __ENDIAN_HPP__

