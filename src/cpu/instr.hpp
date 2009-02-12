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
    explicit instr(uint32_t new_encoded) throw() : encoded(new_encoded) { }
    gpr get_rd() const throw();
    gpr get_rs() const throw();
    gpr get_rt() const throw();
    fpr get_fd() const throw();
    fpr get_fs() const throw();
    fpr get_ft() const throw();
    fpr get_fr() const throw();
    c0r get_c0rd() const throw();
    c2r get_c2rt() const throw();
    hwr get_hwrd() const throw();
    unsigned get_imm() const throw();
    int get_simm() const throw();
    unsigned get_j_tgt() const throw();
    unsigned get_sa() const throw();
    unsigned get_lsb() const throw();
    unsigned get_msb() const throw();
    unsigned get_cc() const throw();
    unsigned get_cond_cc() const throw();
    unsigned get_sel() const throw();
    unsigned get_cofun() const throw();
    unsigned get_cache_op() const throw();
    unsigned get_cachex_op() const throw();
    unsigned get_sync_type() const throw();
    instr_code get_opcode() const throw();
    friend ostream &operator<<(ostream &, const instr &);
private:
    ostream &show_to(ostream &) const;
private:
    unsigned get_bits(unsigned, unsigned) const throw();
    uint32_t encoded;
};

inline gpr instr::get_rd() const throw() { return gpr(get_bits(15,11)); }
inline gpr instr::get_rs() const throw() { return gpr(get_bits(25,21)); }
inline gpr instr::get_rt() const throw() { return gpr(get_bits(20,16)); }
inline fpr instr::get_fd() const throw() { return fpr(get_bits(10,6)); }
inline fpr instr::get_fs() const throw() { return fpr(get_bits(15,11)); }
inline fpr instr::get_ft() const throw() { return fpr(get_bits(20,16)); }
inline fpr instr::get_fr() const throw() { return fpr(get_bits(25,21)); }
inline c0r instr::get_c0rd() const throw() { return c0r(get_bits(15,11)); }
inline c2r instr::get_c2rt() const throw() { return c2r(get_bits(20,16)); }
inline hwr instr::get_hwrd() const throw() { return hwr(get_bits(15,11)); }
inline unsigned instr::get_bits(unsigned hi, unsigned lo) const throw() {
    return bits(encoded, hi, lo);
}
inline instr_code instr::get_opcode() const throw() { return decode(encoded); }
inline unsigned instr::get_imm() const throw() { return get_bits(15,0); }
inline int instr::get_simm() const throw() { return (int16_t) get_imm(); }
inline unsigned instr::get_j_tgt() const throw() { return get_bits(25,0); }
inline unsigned instr::get_sa() const throw() { return get_bits(10,6); }
inline unsigned instr::get_sel() const throw() { return get_bits(2,0); }
inline unsigned instr::get_msb() const throw() { return get_bits(15,11); }
inline unsigned instr::get_lsb() const throw() { return get_bits(10,6); }
inline unsigned instr::get_cc() const throw() { return get_bits(20,18); }
inline unsigned instr::get_cond_cc() const throw() { return get_bits(10,8); }
inline unsigned instr::get_cache_op() const throw() { return get_bits(20,16); }
inline unsigned instr::get_cachex_op() const throw() { return get_bits(15,11); }
inline unsigned instr::get_sync_type() const throw() { return get_bits(10,6); }
inline unsigned instr::get_cofun() const throw() { return get_bits(24,0); }

ostream &operator<<(ostream &, const instr &);

#endif // __INSTR_HPP__

