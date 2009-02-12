// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __INSTR_FORMAT_HPP__
#define __INSTR_FORMAT_HPP__

// instruction formats, for pretty-printing

typedef enum {
    // register
    IF_IMM3,    // e.g., ADDI
    IF_IMM2,    // e.g., LUI
    IF_IMM2_,   // e.g., TEQI
    IF_R3,      // e.g., ADD
    IF_R2,      // e.g., SEB
    IF_R2Z,     // e.g., MOVE
    IF_R2_,     // e.g., DIV
    IF_R1,      // e.g., MFHI
    IF_R1_,     // e.g., JR
    IF_SH,      // e.g., SRL
    IF_SHV,     // e.g., SRLV
    IF_COUNT,   // e.g., CLO
    IF_EXT,     // e.g., EXT
    IF_INS,     // e.g., INS
    // branches and jumps
    IF_B3,      // e.g., BEQ
    IF_B2,      // e.g., BGEZ
    IF_B1,      // e.g., B
    IF_BC,      // e.g., BC1F
    IF_J,       // e.g., J
    IF_JALR,    // e.g., JALR
    // memory
    IF_MEM,     // e.g., LB
    IF_MEM_,    // e.g., SYNCI
    IF_FPMEM,   // e.g., LDC1
    IF_FPLDX,   // e.g., LDXC1
    IF_FPSTX,   // e.g., SDXC1
    // floating point
    IF_FP2,     // e.g., ABS.S
    IF_FP3,     // e.g., ADD.S
    IF_FP4,     // e.g., MADD.S
    IF_FP4G,    // e.g., ALNV.PS
    IF_FPC,     // e.g., C.cond.S
    IF_FPMV,    // e.g., CFC1
    IF_MVCFP,   // e.g., MOVF
    IF_FPMVCFP, // e.g., MOVCF
    IF_FPMVC,   // e.g., MOVN.S
    // coprocessors
    IF_C0MV,    // e.g., MFC0 
    IF_C2OP,    // e.g., COP2
    IF_C2MV,    // e.g., CFC2
    IF_C2MEM,   // e.g., LDC2
    // special
    IF_INT,     // e.g., DI
    IF_RDHW,    // e.g., RDHWR
    IF_CACHE,   // e.g., CACHE
    IF_CACHEX,  // e.g., PREFX
    IF_SYNC,    // e.g., SYNC
    IF_NOARG,   // e.g., SYSCALL
    IF_NUM_INSTR_FORMATS
} instr_format;

#endif // __INSTR_FORMAT_HPP__

