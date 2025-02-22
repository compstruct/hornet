// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __REG_HPP__
#define __REG_HPP__

#include <string>
#include <cassert>
#include "error.hpp"

using namespace std;

class reg {
public:
    explicit reg(unsigned new_reg_no);
    bool operator==(const reg &) const;
    unsigned get_no() const;
protected:
    unsigned reg_no;
};

class gpr : public reg {
public:
    gpr(unsigned new_reg_no);
    gpr(const gpr &copied_reg);
public:
    static bool use_functional_names;
public:
    friend ostream &operator<<(ostream &, const gpr &);
};

class fpr : public reg {
public:
    fpr(unsigned new_reg_no);
    fpr(const fpr &copied_reg);
public:
    friend ostream &operator<<(ostream &, const fpr &);
};

class cfr : public reg { // condition flag register
public:
    cfr(unsigned new_reg_no);
    cfr(const cfr &copied_reg);
public:
    friend ostream &operator<<(ostream &, const cfr &);
};

class c0r : public reg {
public:
    c0r(unsigned new_reg_no);
    c0r(const c0r &copied_reg);
public:
    friend ostream &operator<<(ostream &, const c0r &);
};

class c2r : public reg {
public:
    c2r(unsigned new_reg_no);
    c2r(const c2r &copied_reg);
public:
    friend ostream &operator<<(ostream &, const c2r &);
};

class hwr : public reg {
public:
    hwr(unsigned new_reg_no);
    hwr(const hwr &copied_reg);
public:
    friend ostream &operator<<(ostream &, const hwr &);
};

inline reg::reg(unsigned new_reg_no) : reg_no(new_reg_no) {
    if (new_reg_no > 31) throw err_bad_reg(new_reg_no);
}
inline bool reg::operator==(const reg &r) const {
    return get_no() == r.get_no();
}
inline unsigned reg::get_no() const {
    return reg_no;
}

inline gpr::gpr(unsigned reg_no) : reg(reg_no) { }
inline gpr::gpr(const gpr &orig) : reg(orig.get_no()) { }
ostream &operator<<(ostream &, const gpr &);

inline fpr::fpr(unsigned reg_no) : reg(reg_no) { }
inline fpr::fpr(const fpr &orig) : reg(orig.get_no()) { }
ostream &operator<<(ostream &, const fpr &);

inline cfr::cfr(unsigned reg_no) : reg(reg_no) { }
inline cfr::cfr(const cfr &orig) : reg(orig.get_no()) { }
ostream &operator<<(ostream &, const cfr &);

inline c0r::c0r(unsigned reg_no) : reg(reg_no) { }
inline c0r::c0r(const c0r &orig) : reg(orig.get_no()) { }
ostream &operator<<(ostream &, const c0r &);

inline c2r::c2r(unsigned reg_no) : reg(reg_no) { }
inline c2r::c2r(const c2r &orig) : reg(orig.get_no()) { }
ostream &operator<<(ostream &, const c2r &);

inline hwr::hwr(unsigned reg_no) : reg(reg_no) { }
inline hwr::hwr(const hwr &orig) : reg(orig.get_no()) { }
ostream &operator<<(ostream &, const hwr &);

#endif

