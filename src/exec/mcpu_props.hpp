#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include "logger.hpp"
#include "reg.hpp"
#include "mem.hpp"
#include "bridge.hpp"
#include "pe.hpp"
#include "statistics.hpp"
#include "core.hpp"
#include "catStrip.hpp"
#include "dramController.hpp"
#include "tile.hpp"
#include "cache.hpp"
#include "instr.hpp"

/* Configures instruction execute stage latency in a pipelined, fully-bypassed 
         MIPS core model. 
         @returns: the number of cycles worth of delay the instruction should have,
                                                 return -1 means that the instruction is not currently implemented 
                                                 (will throw an error) */
int get_cycle_delay(shared_ptr<instr> ip) {
    instr i = *ip;
    instr_code code = i.get_opcode();

    switch (code) {
                //              
                // Arithmetic
                //              
    case IC_ADD: 
    case IC_ADDI: 
    case IC_ADDIU: 
    case IC_ADDU: 
    case IC_AND: 
    case IC_ANDI: 
    case IC_NOR: 
    case IC_OR: 
    case IC_ORI: 
    case IC_SLL: 
    case IC_SLLV: 
    case IC_SLT: 
    case IC_SLTI:
    case IC_SLTIU: 
    case IC_SLTU: 
    case IC_SRA: 
    case IC_SRAV: 
    case IC_SRL: 
    case IC_SRLV: 
    case IC_SUB: 
    case IC_SUBU: 
    case IC_XOR:
    case IC_XORI: return 1;
                //
                // Mult/Div
                //
    case IC_DIV: 
    case IC_DIVU: 
    case IC_MUL: 
    case IC_MULT: 
    case IC_MULTU: return 5; 
    case IC_MADD: 
    case IC_MADDU: 
    case IC_MSUB: 
    case IC_MSUBU: return 6;
                //
                // Branch
                //
    case IC_B: 
    case IC_BAL: 
    case IC_BC1F: 
    case IC_BC1FL: 
    case IC_BC1T: 
    case IC_BC1TL: 
    case IC_BEQ: 
    case IC_BEQL: 
    case IC_BGEZ: 
    case IC_BGEZAL: 
    case IC_BGEZALL: 
    case IC_BGEZL: 
    case IC_BGTZ: 
    case IC_BGTZL: 
    case IC_BLEZ: 
    case IC_BLEZL: 
    case IC_BLTZ: 
    case IC_BLTZAL: 
    case IC_BLTZALL: 
    case IC_BLTZL:
    case IC_BNE: 
    case IC_BNEL: return 1;
                //              
                // Jump
                //
    case IC_J: 
    case IC_JAL: 
    case IC_JALR:
    case IC_JALR_HB: 
    case IC_JR:
    case IC_JR_HB: return 1;
                //
                // Memory
                //
    case IC_LB: 
    case IC_LBU: 
    case IC_LH:
    case IC_LHU: 
    case IC_LUI: 
    case IC_LW: 
    case IC_SW: return 1;
                //
                // Floating
                //
        case IC_ADD_D: return 10;
    case IC_C_LT_S: 
    case IC_SUB_S: 
    case IC_ADD_S: return 5;
    case IC_CVT_D_S: return 2;
    case IC_CVT_S_D: return 1;
    case IC_DIV_D:
                case IC_MUL_D: return 20;    
                case IC_DIV_S: 
    case IC_MUL_S: return 10;
    case IC_MOV_S: 
    case IC_MTC1: 
    case IC_MFC1: return 1;
    case IC_MOV_D: return 2;    
    case IC_LWC1: 
    case IC_SWC1: return 1;
    case IC_LDC1: 
    case IC_SDC1: return 2;
    case IC_TRUNC_W_S: return 1;
                //
                // Misc
                //
    case IC_DI: 
    case IC_EHB: 
    case IC_EI: 
    case IC_MFHI: 
    case IC_MFLO: 
    case IC_MOVE: 
    case IC_MOVN: 
    case IC_MOVZ: 
    case IC_MTHI: 
    case IC_MTLO: 
    case IC_RDHWR: 
    case IC_SB: 
    case IC_SEB: 
    case IC_SEH: 
    case IC_EXT: 
    case IC_INS:
    case IC_NOP: 
    case IC_SSNOP: 
    case IC_SYSCALL: // TODO: model intrinsics 
    case IC_TEQ: 
    case IC_SH: return 1;
    case IC_CLO: 
    case IC_CLZ: return 32; // TODO: is this implemented in 1 or 32 cycles?
                //
                // Unimplemented  
                //  
                case IC_ABS_D: return -1;
    case IC_ABS_PS: return -1;
    case IC_ABS_S: return -1;
    case IC_BC2F: return -1;
    case IC_BC2FL: return -1;
    case IC_BC2T: return -1;
    case IC_BC2TL: return -1;
    case IC_ADD_PS: return -1;
    case IC_ALNV_PS: return -1;
    case IC_BREAK: return -1;
    case IC_CACHE: return -1;
    case IC_CEIL_L_D: return -1;
    case IC_CEIL_L_S: return -1;
    case IC_CEIL_W_D: return -1;
    case IC_CEIL_W_S: return -1;
    case IC_CFC1: return -1;
    case IC_CFC2: return -1;
    case IC_COP2: return -1;
    case IC_CTC1: return -1;
    case IC_CTC2: return -1;
    case IC_CVT_D_L: return -1;
    case IC_CVT_D_W: return -1;
    case IC_CVT_L_D: return -1;
    case IC_CVT_L_S: return -1;
    case IC_CVT_PS_S: return -1;
    case IC_CVT_S_L: return -1;
    case IC_CVT_S_PL: return -1;
    case IC_CVT_S_PU: return -1;
    case IC_CVT_S_W: return -1;
    case IC_CVT_W_D: return -1;
    case IC_CVT_W_S: return -1;
    case IC_C_EQ_D: return -1;
    case IC_C_EQ_PS: return -1;
    case IC_C_EQ_S: return -1;
    case IC_C_F_D: return -1;
    case IC_C_F_PS: return -1;
    case IC_C_F_S: return -1;
    case IC_C_LE_D: return -1;
    case IC_C_LE_PS: return -1;
    case IC_C_LE_S: return -1;
    case IC_C_LT_D: return -1;
    case IC_C_LT_PS: return -1;
    case IC_C_NGE_D: return -1;
    case IC_C_NGE_PS: return -1;
    case IC_C_NGE_S: return -1;
    case IC_C_NGLE_D: return -1;
    case IC_C_NGLE_PS: return -1;
    case IC_C_NGLE_S: return -1;
    case IC_C_NGL_D: return -1;
    case IC_C_NGL_PS: return -1;
    case IC_C_NGL_S: return -1;
    case IC_C_NGT_D: return -1;
    case IC_C_NGT_PS: return -1;
    case IC_C_NGT_S: return -1;
    case IC_C_OLE_D: return -1;
    case IC_C_OLE_PS: return -1;
    case IC_C_OLE_S: return -1;
    case IC_C_OLT_D: return -1;
    case IC_C_OLT_PS: return -1;
    case IC_C_OLT_S: return -1;
    case IC_C_SEQ_D: return -1;
    case IC_C_SEQ_PS: return -1;
    case IC_C_SEQ_S: return -1;
    case IC_C_SF_D: return -1;
    case IC_C_SF_PS: return -1;
    case IC_C_SF_S: return -1;
    case IC_C_UEQ_D: return -1;
    case IC_C_UEQ_PS: return -1;
    case IC_C_UEQ_S: return -1;
    case IC_C_ULE_D: return -1;
    case IC_C_ULE_PS: return -1;
    case IC_C_ULE_S: return -1;
    case IC_C_ULT_D: return -1;
    case IC_C_ULT_PS: return -1;
    case IC_C_ULT_S: return -1;
    case IC_C_UN_D: return -1;
    case IC_C_UN_PS: return -1;
    case IC_C_UN_S: return -1;
    case IC_DERET: return -1;
    case IC_ERET: return -1;
    case IC_FLOOR_L_D: return -1;
    case IC_FLOOR_L_S: return -1;
    case IC_FLOOR_W_D: return -1;
    case IC_FLOOR_W_S: return -1;
    case IC_LDC2: return -1;
    case IC_LDXC1: return -1;
    case IC_LL: return -1;
    case IC_LUXC1: return -1;
    case IC_LWC2: return -1;
    case IC_LWL: return -1;
    case IC_LWR: return -1;
    case IC_LWXC1: return -1;
    case IC_MADD_D: return -1;
    case IC_MADD_PS: return -1;
    case IC_MADD_S: return -1;
    case IC_MFC0: return -1;
    case IC_MFC2: return -1;
    case IC_MFHC1: return -1;
    case IC_MFHC2: return -1;
    case IC_MOVF: return -1;
    case IC_MOVF_D: return -1;
    case IC_MOVF_PS: return -1;
    case IC_MOVF_S: return -1;
    case IC_MOVN_D: return -1;
    case IC_MOVN_PS: return -1;
    case IC_MOVN_S: return -1;
    case IC_MOVT: return -1;
    case IC_MOVT_D: return -1;
    case IC_MOVT_PS: return -1;
    case IC_MOVT_S: return -1;
    case IC_MOVZ_D: return -1;
    case IC_MOVZ_PS: return -1;
    case IC_MOVZ_S: return -1;
    case IC_MOV_PS: return -1;
    case IC_MSUB_D: return -1;
    case IC_MSUB_PS: return -1;
    case IC_MSUB_S: return -1;
    case IC_MTC0: return -1;
    case IC_MTC2: return -1;
    case IC_MTHC1: return -1;
    case IC_MTHC2: return -1;
    case IC_MUL_PS: return -1;
    case IC_NEG_D: return -1;
    case IC_NEG_PS: return -1;
    case IC_NEG_S: return -1;
    case IC_NMADD_D: return -1;
    case IC_NMADD_PS: return -1;
    case IC_NMADD_S: return -1;
    case IC_NMSUB_D: return -1;
    case IC_NMSUB_PS: return -1;
    case IC_NMSUB_S: return -1;
    case IC_PAUSE: return -1;
    case IC_PLL_PS: return -1;
    case IC_PLU_PS: return -1;
    case IC_PREF: return -1;
    case IC_PREFX: return -1;
    case IC_PUL_PS: return -1;
    case IC_PUU_PS: return -1;
    case IC_RDPGPR: return -1;
    case IC_RECIP_D: return -1;
    case IC_RECIP_S: return -1;
    case IC_ROTR: return -1; // XXX
    case IC_ROTRV: return -1; // XXX
    case IC_ROUND_L_D: return -1;
    case IC_ROUND_L_S: return -1;
    case IC_ROUND_W_D: return -1;
    case IC_ROUND_W_S: return -1;
    case IC_RSQRT_D: return -1;
    case IC_RSQRT_S: return -1;
    case IC_SC: return -1;
    case IC_SDBBP: return -1;
    case IC_SDC2: return -1;
    case IC_SDXC1: return -1;
    case IC_SQRT_D: return -1;
    case IC_SQRT_S: return -1;
    case IC_SUB_PS: return -1;
    case IC_SUXC1: return -1;
    case IC_SWC2: return -1;
    case IC_SWL: return -1;
    case IC_SWR: return -1;
    case IC_SWXC1: return -1;
    case IC_SYNC: return -1;
    case IC_SYNCI: return -1;
    case IC_TEQI: return -1;
    case IC_TGE: return -1;
    case IC_TGEI: return -1;
    case IC_TGEIU: return -1;
    case IC_TGEU: return -1;
    case IC_TLBP: return -1;
    case IC_TLBR: return -1;
    case IC_TLBWI: return -1;
    case IC_TLBWR: return -1;
    case IC_TLT: return -1;
    case IC_TLTI: return -1;
    case IC_TLTIU: return -1;
    case IC_TLTU: return -1;
    case IC_TNE: return -1;
    case IC_TNEI: return -1;
    case IC_TRUNC_L_D: return -1;
    case IC_TRUNC_L_S: return -1;
    case IC_TRUNC_W_D: return -1;
    case IC_WAIT: return -1;
    case IC_WRPGPR: return -1;
    case IC_WSBH: return -1;
    default: return -1;
    }
}
