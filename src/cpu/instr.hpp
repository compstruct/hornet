// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __INSTR_HPP__
#define __INSTR_HPP__

#include "cstdint.hpp"
#include "bits.hpp"
#include "reg.hpp"
#include "instr_encoding.hpp"

class instr {
public:
    explicit instr(uint32_t new_encoded) : encoded(new_encoded) { }
    gpr get_rd() const;
    gpr get_rs() const;
    gpr get_rt() const;
    fpr get_fd() const;
    fpr get_fs() const;
    fpr get_ft() const;
    fpr get_fr() const;
    c0r get_c0rd() const;
    c2r get_c2rt() const;
    hwr get_hwrd() const;
    unsigned get_imm() const;
    int get_simm() const;
    unsigned get_j_tgt() const;
    unsigned get_sa() const;
    unsigned get_lsb() const;
    unsigned get_msb() const;
    unsigned get_cc() const;
    unsigned get_cond_cc() const;
    unsigned get_sel() const;
    unsigned get_cofun() const;
    unsigned get_cache_op() const;
    unsigned get_cachex_op() const;
    unsigned get_sync_type() const;
    instr_code get_opcode() const;
    friend ostream &operator<<(ostream &, const instr &);
private:
    ostream &show_to(ostream &) const;
private:
    unsigned get_bits(unsigned, unsigned) const;
    uint32_t encoded;
};

inline gpr instr::get_rd() const { return gpr(get_bits(15,11)); }
inline gpr instr::get_rs() const { return gpr(get_bits(25,21)); }
inline gpr instr::get_rt() const { return gpr(get_bits(20,16)); }
inline fpr instr::get_fd() const { return fpr(get_bits(10,6)); }
inline fpr instr::get_fs() const { return fpr(get_bits(15,11)); }
inline fpr instr::get_ft() const { return fpr(get_bits(20,16)); }
inline fpr instr::get_fr() const { return fpr(get_bits(25,21)); }
inline c0r instr::get_c0rd() const { return c0r(get_bits(15,11)); }
inline c2r instr::get_c2rt() const { return c2r(get_bits(20,16)); }
inline hwr instr::get_hwrd() const { return hwr(get_bits(15,11)); }
inline unsigned instr::get_bits(unsigned hi, unsigned lo) const {
    return bits(encoded, hi, lo);
}
inline instr_code instr::get_opcode() const { return decode(encoded); }
inline unsigned instr::get_imm() const { return get_bits(15,0); }
inline int instr::get_simm() const { return (int16_t) get_imm(); }
inline unsigned instr::get_j_tgt() const { return get_bits(25,0); }
inline unsigned instr::get_sa() const { return get_bits(10,6); }
inline unsigned instr::get_sel() const { return get_bits(2,0); }
inline unsigned instr::get_msb() const { return get_bits(15,11); }
inline unsigned instr::get_lsb() const { return get_bits(10,6); }
inline unsigned instr::get_cc() const { return get_bits(20,18); }
inline unsigned instr::get_cond_cc() const { return get_bits(10,8); }
inline unsigned instr::get_cache_op() const { return get_bits(20,16); }
inline unsigned instr::get_cachex_op() const { return get_bits(15,11); }
inline unsigned instr::get_sync_type() const { return get_bits(10,6); }
inline unsigned instr::get_cofun() const { return get_bits(24,0); }

ostream &operator<<(ostream &, const instr &);

#endif // __INSTR_HPP__

