// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <iostream>
#include <iomanip>
#include "instr_format.hpp"
#include "instr_encoding.hpp"
#include "instr.hpp"

ostream &instr::show_to(ostream &out) const {
    instr_code code = get_opcode();
    instr_format fmt = ic_infos[code].format;
    out << ic_infos[code].name << " ";
    out << setfill('0') << dec << noshowpos;
    switch (fmt) {
    // register-immediate
    case IF_IMM3:    // e.g.: ADDI
        out << get_rt() << ", " << get_rs() << ", " << get_simm();
        break;
    case IF_IMM2:    // e.g.: LUI
        out << get_rt() << ", " << get_simm();
        break;
    case IF_IMM2_:   // e.g.: TEQI
        out << get_rs() << ", " << get_simm();
        break;
    // register-register
    case IF_R3:      // e.g.: ADD
        out << get_rd() << ", " << get_rs() << ", " << get_rt();
        break;
    case IF_R2:      // e.g.: SEB
        out << get_rd() << ", " << get_rt();
        break;
    case IF_R2Z:     // e.g.: MOVE
        out << get_rd() << ", " << get_rs();
        break;
    case IF_R2_:     // e.g.: DIV
        out << get_rs() << ", " << get_rt();
        break;
    case IF_R1:      // e.g.: MFHI
        out << get_rd();
        break;
    case IF_R1_:     // e.g.: JR
        out << get_rs();
        break;
    case IF_SH:      // e.g.: SRL
        out << get_rd() << ", " << get_rt() << ", " << get_sa();
        break;
    case IF_SHV:     // e.g.: SRLV
        out << get_rd() << ", " << get_rt() << ", " << get_rs();
        break;
    case IF_COUNT:   // e.g.: CLO
        out << get_rd() << ", " << get_rs();
        break;
    case IF_EXT:     // e.g.: EXT
        out << get_rt() << ", " << get_rs() << ", " << get_lsb() << ", "
            << get_msb() + 1;
        break;
    case IF_INS:     // e.g.: INS
        out << get_rt() << ", " << get_rs() << ", " << get_lsb() << ", "
            << get_msb() - get_lsb() + 1;
        break;
    // branches and jumps
    case IF_B3:      // e.g.: BEQ
        out << get_rs() << ", " << get_rt() << ", "
            << showpos << (get_simm() << 2) << noshowpos;
        break;
    case IF_B2:      // e.g.: BGEZ
        out << get_rs() << ", " << showpos << (get_simm() << 2) << noshowpos;
        break;
    case IF_B1:      // e.g.: B
        out << showpos << (get_simm() << 2) << noshowpos;
        break;
    case IF_BC:      // e.g.: BC1F
        if (get_cc() == 0)
            out << showpos << (get_simm() << 2) << noshowpos;
        else
            out << get_cc() << ", " << showpos << (get_simm() << 2)
                << noshowpos;
        break;
    case IF_J:       // e.g.: J
        out << hex << setw(8) << (get_j_tgt() << 2) << dec;
        break;
    case IF_JALR:    // e.g.: JALR
        if (get_rd() == gpr(31)) out << get_rd() << ", " << get_rs();
        else out << get_rs();
        break;
    // memory
    case IF_MEM:     // e.g.: LB
        out << get_rt() << ", " << get_simm() << "(" << get_rs() << ")";
        break;
    case IF_MEM_:    // e.g.: SYNCI
        out << get_simm() << "(" << get_rs() << ")";
        break;
    case IF_FPMEM:   // e.g.: LDC1
        out << get_ft() << ", " << get_simm() << "(" << get_rs() << ")";
        break;
    case IF_FPLDX:   // e.g.: LDXC1
        out << get_fd() << ", " << get_rt() << "(" << get_rs() << ")";
        break;
    case IF_FPSTX:   // e.g.: SDXC1
        out << get_fs() << ", " << get_rt() << "(" << get_rs() << ")";
        break;
    // floating point
    case IF_FP4:     // e.g.: MADD.S
        out << get_fd() << ", " << get_fr() << ", " << get_ft() << ", "
            << get_fs();
        break;
    case IF_FP4G:    // e.g.: ALNV.PS
        out << get_fd() << ", " << get_fs() << ", " << get_ft() << ", "
            << get_rs();
        break;
    case IF_FP3:     // e.g.: ADD.S
        out << get_fd() << ", " << get_fs() << ", " << get_ft();
        break;
    case IF_FP2:     // e.g.: ABS.S
        out << get_fd() << ", " << get_fs();
        break;
    case IF_FPC:     // e.g.: C.cond.S
        if (get_cond_cc() == 0) out << get_fs() << ", " << get_ft();
        else out << get_cc() << ", " << get_fs() << ", " << get_ft();
        break;
    case IF_FPMV:    // e.g.: CFC1
        out << get_rt() << ", " << get_fs();
        break;
    case IF_MVCFP:   // e.g.: MOVF
        out << get_rd() << ", " << get_rs() << ", " << get_cc();
        break;
    case IF_FPMVCFP: // e.g.: MOVCF
        out << get_fd() << ", " << get_fs() << ", " << get_cc();
        break;
    case IF_FPMVC:   // e.g.: MOVN.S
        out << get_fd() << ", " << get_fs() << ", " << get_rt();
        break;
    // coprocessors
    case IF_C0MV:    // e.g.: MFC0 
        out << get_rt() << ", " << get_c0rd() << ", " << get_sel();
        break;
    case IF_C2OP:    // e.g.: COP2
        out << "0x" << hex << setw(7) << get_cofun();
        break;
    case IF_C2MV:    // e.g.: CFC2
        out << get_rt() << ", " << "impl 0x" << hex << setw(4) << get_imm();
        break;
    case IF_C2MEM:   // e.g.: LDC2
        out << get_c2rt() << ", " << get_simm() << "(" << get_rs() << ")";
        break;
    // special
    case IF_INT:     // e.g.: DI
        out << get_rt();
        break;
    case IF_RDHW:    // e.g.: RDHWR
        out << get_rt() << ", " << get_hwrd();
        break;
    case IF_CACHE:   // e.g.: CACHE
        out << get_cache_op() << ", " << get_simm()
            << "(" << get_rs() << ")";
        break;
    case IF_CACHEX:  // e.g.: PREFX
        out << get_cachex_op() << ", " << get_rt()
            << "(" << get_rs() << ")";
        break;
    case IF_SYNC:    // e.g.: SYNC
        if (get_sync_type())
            out << get_sync_type();
        break;
    case IF_NOARG:   // e.g.: SYSCALL
        break;
    default:
        throw err_panic("bad instruction format");
    }
    return out;
}

ostream &operator<<(ostream &out, const instr &i) {
    return i.show_to(out);
}

