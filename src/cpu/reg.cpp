// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iostream>
#include "reg.hpp"

bool gpr::use_functional_names = true;

ostream &operator<<(ostream &out, const gpr &r) {
    assert(r.reg_no >= 0);
    assert(r.reg_no <= 31);
    out << dec;
    if (!gpr::use_functional_names) {
        out << "r" << r.reg_no;
    } else {
        if (r.reg_no == 0) {
            out << "zero";
        } else if (r.reg_no == 1) {
            out << "at";
        } else if (r.reg_no >= 2 && r.reg_no <= 3) {
            out << "v" << (r.reg_no - 2);
        } else if (r.reg_no >= 4 && r.reg_no <= 7) {
            out << "a" << (r.reg_no - 4);
        } else if (r.reg_no >= 8 && r.reg_no <= 15) {
            out << "t" << (r.reg_no - 8);
        } else if (r.reg_no >= 16 && r.reg_no <= 23) {
            out << "s" << (r.reg_no - 16);
        } else if (r.reg_no >= 24 && r.reg_no <= 25) {
            out << "t" << (r.reg_no - 16);
        } else if (r.reg_no >= 26 && r.reg_no <= 27) {
            out << "k" << (r.reg_no - 26);
        } else if (r.reg_no == 28) {
            out << "gp";
        } else if (r.reg_no == 29) {
            out << "sp";
        } else if (r.reg_no == 30) {
            out << "fp";
        } else { // (r.reg_no == 31)
            out << "ra";
        }
    }
    return out;
}

ostream &operator<<(ostream &out, const fpr &r) {
    assert(r.reg_no >= 0);
    assert(r.reg_no <= 31);
    return out << "f" << dec << r.reg_no;
}

ostream &operator<<(ostream &out, const c0r &r) {
    assert(r.reg_no >= 0);
    assert(r.reg_no <= 31);
    return out << "c0r" << dec << r.reg_no;
}

ostream &operator<<(ostream &out, const c2r &r) {
    assert(r.reg_no >= 0);
    assert(r.reg_no <= 31);
    return out << "c2r" << r.reg_no;
}

ostream &operator<<(ostream &out, const hwr &r) { // XXX they have names
    assert(r.reg_no >= 0);
    assert(r.reg_no <= 31);
    switch(r.reg_no) {
    case 0: return out << "cpu_num";
    case 1: return out << "synci_step";
    case 2: return out << "cycle_ctr";
    case 3: return out << "cycle_ctr_res";
    case 4: return out << "cycle_ctr_res";
    case 29: return out << "ulr";
    default: return out << "hwr" << r.reg_no;
    }
}

