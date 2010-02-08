// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include "endian.hpp"
#include "instr.hpp"
#include "syscalls.hpp"
#include "cpu.hpp"

using namespace std;

cpu::cpu(const pe_id &new_id, const uint64_t &new_time, shared_ptr<mem> new_ram,
         uint32_t entry_point, uint32_t stack_ptr,
         logger &l, shared_ptr<vcd_writer> v) throw(err)
    : pe(new_id), running(true), time(new_time), pc(entry_point), ram(new_ram),
      net(), jump_active(false), interrupts_enabled(false), stdout_buffer(),
      log(l), vcd(v) {
    assert(ram);
    pc = entry_point;
    gprs[29] = stack_ptr;
    LOG(log,3) << "cpu " << get_id() << " created with entry point at "
               << hex << setfill('0') << setw(8) << pc << " and stack pointer at "
               << setw(8) << stack_ptr << endl;
}

cpu::~cpu() throw() { }

void cpu::connect(shared_ptr<bridge> net_bridge) throw() { net = net_bridge; }

void cpu::tick_positive_edge() throw(err) {
    if (running) {
        assert(ram);
        execute();
        if (jump_active && (jump_time == time)) {
            LOG(log,5) << "[cpu " << get_id() << "]     pc <- "
                    << hex << setfill('0') << setw(8) << jump_target << endl;
            pc = jump_target;
        } else {
            pc += 4;
        }
    }
}

void cpu::tick_negative_edge() throw(err) { }

bool cpu::is_drained() const throw() {
    return !running;
}

void cpu::flush_stdout() throw() {
    if (!stdout_buffer.str().empty()) {
        LOG(log,0) << "[cpu " << get_id() << " out] " << stdout_buffer.str()
                   << flush;
        stdout_buffer.str("");
    }
}

void cpu::syscall(uint32_t call_no) throw(err) {
    switch (call_no) {
    case SYSCALL_PRINT_INT:
        stdout_buffer << dec << (int32_t) get(gpr(4)); break;
    case SYSCALL_PRINT_STRING: {
        assert(ram);
        string s(reinterpret_cast<char *>(ram->ptr(get(gpr(4)))));
        string::size_type last_pos = 0;
        string::size_type eol_pos = 0;
        while ((eol_pos = s.find('\n', last_pos)) != string::npos) {
            stdout_buffer << string(s, last_pos, eol_pos + 1);
            flush_stdout();
            last_pos = eol_pos + 1;
        }
        stdout_buffer << string(s, last_pos);
        break;
    }
    case SYSCALL_EXIT_SUCCESS:
        flush_stdout(); running = false; break;
    case SYSCALL_EXIT:
        flush_stdout(); running = false; break;
    case SYSCALL_SEND:
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->send(get(gpr(4)), ram->ptr(get(gpr(5))),
                              ((get(gpr(6)) >> 3) +
                               ((get(gpr(6)) & 0x7) != 0 ? 1 : 0))));
        break;
    case SYSCALL_RECEIVE:
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->receive(ram->ptr(get(gpr(5))), get(gpr(4)),
                                 get(gpr(6)) >> 3));
        break;
    case SYSCALL_TRANSMISSION_DONE:
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->get_transmission_done(get(gpr(4)))); break;
    case SYSCALL_WAITING_QUEUES:
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->get_waiting_queues()); break;
    case SYSCALL_PACKET_FLOW:
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->get_queue_flow_id(get(gpr(4)))); break;
    case SYSCALL_PACKET_LENGTH:
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->get_queue_length(get(gpr(4))) << 3); break;
    default: flush_stdout(); throw exc_bad_syscall(call_no);
    }
}

static void unimplemented_instr(instr i, uint32_t addr) throw(err_tbd) {
    ostringstream oss;
    oss << "[0x" << hex << setfill('0') << setw(8) << addr << "] " << i;
    throw err_tbd(oss.str());
}

inline uint32_t check_align(uint32_t addr, uint32_t mask) {
    if (addr & mask) throw exc_addr_align();
    return addr;
}

#define op3sr(op) { set(i.get_rd(), (int32_t) get(i.get_rs()) op \
                                    (int32_t) get(i.get_rt())); }
#define op3ur(op) { set(i.get_rd(), get(i.get_rs()) op get(i.get_rt())); }
#define op3si(op) { set(i.get_rt(), static_cast<int>(get(i.get_rs())) op i.get_simm()); }
#define op3ui(op) { set(i.get_rt(), get(i.get_rs()) op i.get_imm()); }
#define op3ur_of(op) { \
        uint64_t x = get(i.get_rs()); x |= (x & 0x80000000) << 1; \
        uint64_t y = get(i.get_rt()); y |= (y & 0x80000000) << 1; \
        uint64_t v = x op y; \
        if (bits(v,32,32) != bits(v,31,31)) throw exc_int_overflow(); \
        set(i.get_rd(), uint32_t(v)); }
#define op3si_of(op) { \
        uint64_t x = get(i.get_rs()); x |= (x & 0x80000000) << 1; \
        uint64_t y = i.get_simm(); y |= (y & 0x80000000) << 1; \
        uint64_t v = x op y; \
        if (bits(v,32,32) != bits(v,31,31)) throw exc_int_overflow(); \
        set(i.get_rd(), uint32_t(v)); }
#define branch() { jump_active = true; jump_time = time + 1; \
                   jump_target = pc + 4 + (i.get_simm() << 2); }
#define jump(addr) { jump_active = true; jump_time = time + 1; \
                     jump_target = addr; }
#define branch_link() { set(gpr(31), pc + 8); branch(); }
#define branch_now() { pc += (i.get_simm() << 2); }
#define branch_link_now() { set(gpr(31), pc + 8); branch_now(); }
#define mem_addr() (get(i.get_rs()) + i.get_simm())
#define mem_addrh() (check_align(mem_addr(), 0x1))
#define mem_addrw() (check_align(mem_addr(), 0x3))

void cpu::execute() throw(err) {
    assert(ram);
    instr i = instr(ram->load<uint32_t>(pc));
    LOG(log,5) << "[cpu " << get_id() << "] "
               << hex << setfill('0') << setw(8) << pc << ": "
               << i << endl;
    instr_code code = i.get_opcode();
    switch (code) {
    case IC_ABS_D: unimplemented_instr(i, pc);
    case IC_ABS_PS: unimplemented_instr(i, pc);
    case IC_ABS_S: unimplemented_instr(i, pc);
    case IC_ADD: op3ur_of(+); break;
    case IC_ADDI: op3si_of(+); break;
    case IC_ADDIU: op3si(+); break;
    case IC_ADDU: op3ur(+); break;
    case IC_ADD_D: unimplemented_instr(i, pc);
    case IC_ADD_PS: unimplemented_instr(i, pc);
    case IC_ADD_S: unimplemented_instr(i, pc);
    case IC_ALNV_PS: unimplemented_instr(i, pc);
    case IC_AND: op3ur(&); break;
    case IC_ANDI: op3ui(&); break;
    case IC_B: branch(); break;
    case IC_BAL: branch_link(); break;
    case IC_BC1F: unimplemented_instr(i, pc);
    case IC_BC1FL: unimplemented_instr(i, pc);
    case IC_BC1T: unimplemented_instr(i, pc);
    case IC_BC1TL: unimplemented_instr(i, pc);
    case IC_BC2F: unimplemented_instr(i, pc);
    case IC_BC2FL: unimplemented_instr(i, pc);
    case IC_BC2T: unimplemented_instr(i, pc);
    case IC_BC2TL: unimplemented_instr(i, pc);
    case IC_BEQ: if (get(i.get_rs()) == get(i.get_rt())) branch(); break;
    case IC_BEQL: if (get(i.get_rs()) == get(i.get_rt())) branch_now(); break;
    case IC_BGEZ: if ((int32_t) get(i.get_rs()) >= 0) branch(); break;
    case IC_BGEZAL: if ((int32_t) get(i.get_rs()) >= 0) branch_link(); break;
    case IC_BGEZALL: if ((int32_t) get(i.get_rs()) >= 0) branch_link_now(); break;
    case IC_BGEZL: if ((int32_t) get(i.get_rs()) >= 0) branch_now(); break;
    case IC_BGTZ: if ((int32_t) get(i.get_rs()) > 0) branch(); break;
    case IC_BGTZL: if ((int32_t) get(i.get_rs()) > 0) branch_now(); break;
    case IC_BLEZ: if ((int32_t) get(i.get_rs()) <= 0) branch(); break;
    case IC_BLEZL: if ((int32_t) get(i.get_rs()) <= 0) branch_now(); break;
    case IC_BLTZ: if ((int32_t) get(i.get_rs()) < 0) branch(); break;
    case IC_BLTZAL: if ((int32_t) get(i.get_rs()) < 0) branch_link(); break;
    case IC_BLTZALL: if ((int32_t) get(i.get_rs()) < 0) branch_link_now(); break;
    case IC_BLTZL: if ((int32_t) get(i.get_rs()) < 0) branch_now(); break;
    case IC_BNE: if (get(i.get_rs()) != get(i.get_rt())) branch(); break;
    case IC_BNEL: if (get(i.get_rs()) != get(i.get_rt())) branch_now(); break;
    case IC_BREAK: unimplemented_instr(i, pc);
    case IC_CACHE: unimplemented_instr(i, pc);
    case IC_CEIL_L_D: unimplemented_instr(i, pc);
    case IC_CEIL_L_S: unimplemented_instr(i, pc);
    case IC_CEIL_W_D: unimplemented_instr(i, pc);
    case IC_CEIL_W_S: unimplemented_instr(i, pc);
    case IC_CFC1: unimplemented_instr(i, pc);
    case IC_CFC2: unimplemented_instr(i, pc);
    case IC_CLO: {
        uint32_t r = get(i.get_rs());
        uint32_t count = 0;
        for (uint32_t m = 0x80000000; m & r; m >>= 1) ++count;
        set(i.get_rd(), count);
    }
    case IC_CLZ: {
        uint32_t r = get(i.get_rs());
        uint32_t count = 0;
        for (uint32_t m = 0x80000000; m & !(m & r); m >>= 1) ++count;
        set(i.get_rd(), count);
    }
    case IC_COP2: unimplemented_instr(i, pc);
    case IC_CTC1: unimplemented_instr(i, pc);
    case IC_CTC2: unimplemented_instr(i, pc);
    case IC_CVT_D_L: unimplemented_instr(i, pc);
    case IC_CVT_D_S: unimplemented_instr(i, pc);
    case IC_CVT_D_W: unimplemented_instr(i, pc);
    case IC_CVT_L_D: unimplemented_instr(i, pc);
    case IC_CVT_L_S: unimplemented_instr(i, pc);
    case IC_CVT_PS_S: unimplemented_instr(i, pc);
    case IC_CVT_S_D: unimplemented_instr(i, pc);
    case IC_CVT_S_L: unimplemented_instr(i, pc);
    case IC_CVT_S_PL: unimplemented_instr(i, pc);
    case IC_CVT_S_PU: unimplemented_instr(i, pc);
    case IC_CVT_S_W: unimplemented_instr(i, pc);
    case IC_CVT_W_D: unimplemented_instr(i, pc);
    case IC_CVT_W_S: unimplemented_instr(i, pc);
    case IC_C_EQ_D: unimplemented_instr(i, pc);
    case IC_C_EQ_PS: unimplemented_instr(i, pc);
    case IC_C_EQ_S: unimplemented_instr(i, pc);
    case IC_C_F_D: unimplemented_instr(i, pc);
    case IC_C_F_PS: unimplemented_instr(i, pc);
    case IC_C_F_S: unimplemented_instr(i, pc);
    case IC_C_LE_D: unimplemented_instr(i, pc);
    case IC_C_LE_PS: unimplemented_instr(i, pc);
    case IC_C_LE_S: unimplemented_instr(i, pc);
    case IC_C_LT_D: unimplemented_instr(i, pc);
    case IC_C_LT_PS: unimplemented_instr(i, pc);
    case IC_C_LT_S: unimplemented_instr(i, pc);
    case IC_C_NGE_D: unimplemented_instr(i, pc);
    case IC_C_NGE_PS: unimplemented_instr(i, pc);
    case IC_C_NGE_S: unimplemented_instr(i, pc);
    case IC_C_NGLE_D: unimplemented_instr(i, pc);
    case IC_C_NGLE_PS: unimplemented_instr(i, pc);
    case IC_C_NGLE_S: unimplemented_instr(i, pc);
    case IC_C_NGL_D: unimplemented_instr(i, pc);
    case IC_C_NGL_PS: unimplemented_instr(i, pc);
    case IC_C_NGL_S: unimplemented_instr(i, pc);
    case IC_C_NGT_D: unimplemented_instr(i, pc);
    case IC_C_NGT_PS: unimplemented_instr(i, pc);
    case IC_C_NGT_S: unimplemented_instr(i, pc);
    case IC_C_OLE_D: unimplemented_instr(i, pc);
    case IC_C_OLE_PS: unimplemented_instr(i, pc);
    case IC_C_OLE_S: unimplemented_instr(i, pc);
    case IC_C_OLT_D: unimplemented_instr(i, pc);
    case IC_C_OLT_PS: unimplemented_instr(i, pc);
    case IC_C_OLT_S: unimplemented_instr(i, pc);
    case IC_C_SEQ_D: unimplemented_instr(i, pc);
    case IC_C_SEQ_PS: unimplemented_instr(i, pc);
    case IC_C_SEQ_S: unimplemented_instr(i, pc);
    case IC_C_SF_D: unimplemented_instr(i, pc);
    case IC_C_SF_PS: unimplemented_instr(i, pc);
    case IC_C_SF_S: unimplemented_instr(i, pc);
    case IC_C_UEQ_D: unimplemented_instr(i, pc);
    case IC_C_UEQ_PS: unimplemented_instr(i, pc);
    case IC_C_UEQ_S: unimplemented_instr(i, pc);
    case IC_C_ULE_D: unimplemented_instr(i, pc);
    case IC_C_ULE_PS: unimplemented_instr(i, pc);
    case IC_C_ULE_S: unimplemented_instr(i, pc);
    case IC_C_ULT_D: unimplemented_instr(i, pc);
    case IC_C_ULT_PS: unimplemented_instr(i, pc);
    case IC_C_ULT_S: unimplemented_instr(i, pc);
    case IC_C_UN_D: unimplemented_instr(i, pc);
    case IC_C_UN_PS: unimplemented_instr(i, pc);
    case IC_C_UN_S: unimplemented_instr(i, pc);
    case IC_DERET: unimplemented_instr(i, pc);
    case IC_DI: interrupts_enabled = false;
    case IC_DIV: {
        int32_t rhs = get(i.get_rt());
        if (rhs != 0) {
            int32_t lhs = get(i.get_rs());
            set_hi_lo((((uint64_t) (lhs%rhs)) << 32) | ((uint32_t) (lhs/rhs)));
        }
        break;
    }
    case IC_DIVU: {
        uint32_t rhs = get(i.get_rt());
        if (rhs != 0) {
            uint32_t lhs = get(i.get_rs());
            set_hi_lo((((uint64_t) (lhs % rhs)) << 32) | (lhs / rhs));
        }
        break;
    }
    case IC_DIV_D: unimplemented_instr(i, pc);
    case IC_DIV_S: unimplemented_instr(i, pc);
    case IC_EHB: break;
    case IC_EI: interrupts_enabled = true;
    case IC_ERET: unimplemented_instr(i, pc);
    case IC_EXT: set(i.get_rt(), bits(get(i.get_rs()),i.get_msb(),i.get_lsb()));
                 break;
    case IC_FLOOR_L_D: unimplemented_instr(i, pc);
    case IC_FLOOR_L_S: unimplemented_instr(i, pc);
    case IC_FLOOR_W_D: unimplemented_instr(i, pc);
    case IC_FLOOR_W_S: unimplemented_instr(i, pc);
    case IC_INS: set(i.get_rt(), splice(get(i.get_rt()), get(i.get_rs()),
                                        i.get_msb(), i.get_lsb()));
                 break;
    case IC_J: jump(((pc + 4) & 0xf0000000) | (i.get_j_tgt() << 2)); break;
    case IC_JAL: set(gpr(31), pc + 8);
                 jump(((pc + 4) & 0xf0000000) | (i.get_j_tgt() << 2));
                 break;
    case IC_JALR:
    case IC_JALR_HB: set(i.get_rd(), pc + 8); jump(get(i.get_rs())); break;
    case IC_JR:
    case IC_JR_HB: set(i.get_rd(), pc + 8); jump(get(i.get_rs())); break;
    case IC_LB: set(i.get_rt(), (int32_t) load<int8_t>(mem_addr())); break;
    case IC_LBU: set(i.get_rt(), load<uint8_t>(mem_addr())); break;
    case IC_LDC1: unimplemented_instr(i, pc);
    case IC_LDC2: unimplemented_instr(i, pc);
    case IC_LDXC1: unimplemented_instr(i, pc);
    case IC_LH: set(i.get_rt(), (int32_t) load<int16_t>(mem_addrh()));
                break;
    case IC_LHU: set(i.get_rt(), load<uint16_t>(mem_addrh())); break;
    case IC_LL: unimplemented_instr(i, pc);
    case IC_LUI: set(i.get_rt(), i.get_imm() << 16); break;
    case IC_LUXC1: unimplemented_instr(i, pc);
    case IC_LW: set(i.get_rt(), load<uint32_t>(mem_addrw())); break;
    case IC_LWC1: unimplemented_instr(i, pc);
    case IC_LWC2: unimplemented_instr(i, pc);
    case IC_LWL: {
        uint32_t a = mem_addr();
        uint32_t w = load<uint32_t>(a);
        uint32_t m = 0xffffffffU << ((a & 0x3) << 3);
        set(i.get_rt(), combine(get(i.get_rt()), w, m));
        break;
    }
    case IC_LWR: {
        uint32_t a = mem_addr();
        uint32_t w = load<uint32_t>(a - 3);
        uint32_t m = ~(0xffffff00U << ((a & 0x3) << 3));
        set(i.get_rt(), combine(get(i.get_rt()), w, m));
        break;
    }
    case IC_LWXC1: unimplemented_instr(i, pc);
    case IC_MADD: set_hi_lo(hi_lo + ((int64_t) ((int32_t) get(i.get_rs())) *
                                     (int64_t) ((int32_t) get(i.get_rt()))));
                  break;
    case IC_MADDU: set_hi_lo(hi_lo + ((uint64_t) get(i.get_rs()) *
                                      (uint64_t) get(i.get_rt())));
                   break;
    case IC_MADD_D: unimplemented_instr(i, pc);
    case IC_MADD_PS: unimplemented_instr(i, pc);
    case IC_MADD_S: unimplemented_instr(i, pc);
    case IC_MFC0: unimplemented_instr(i, pc);
    case IC_MFC1: unimplemented_instr(i, pc);
    case IC_MFC2: unimplemented_instr(i, pc);
    case IC_MFHC1: unimplemented_instr(i, pc);
    case IC_MFHC2: unimplemented_instr(i, pc);
    case IC_MFHI: set(i.get_rd(), hi_lo >> 32); break;
    case IC_MFLO: set(i.get_rd(), hi_lo & 0xffffffffU); break;
    case IC_MOVE: set(i.get_rd(), get(i.get_rs())); break;
    case IC_MOVF: unimplemented_instr(i, pc);
    case IC_MOVF_D: unimplemented_instr(i, pc);
    case IC_MOVF_PS: unimplemented_instr(i, pc);
    case IC_MOVF_S: unimplemented_instr(i, pc);
    case IC_MOVN: if (get(i.get_rt()) != 0) set(i.get_rd(), get(i.get_rs()));
                  break;
    case IC_MOVN_D: unimplemented_instr(i, pc);
    case IC_MOVN_PS: unimplemented_instr(i, pc);
    case IC_MOVN_S: unimplemented_instr(i, pc);
    case IC_MOVT: unimplemented_instr(i, pc);
    case IC_MOVT_D: unimplemented_instr(i, pc);
    case IC_MOVT_PS: unimplemented_instr(i, pc);
    case IC_MOVT_S: unimplemented_instr(i, pc);
    case IC_MOVZ: if (get(i.get_rt()) == 0) set(i.get_rd(), get(i.get_rs()));
                  break;
    case IC_MOVZ_D: unimplemented_instr(i, pc);
    case IC_MOVZ_PS: unimplemented_instr(i, pc);
    case IC_MOVZ_S: unimplemented_instr(i, pc);
    case IC_MOV_D: unimplemented_instr(i, pc);
    case IC_MOV_PS: unimplemented_instr(i, pc);
    case IC_MOV_S: unimplemented_instr(i, pc);
    case IC_MSUB: set_hi_lo(hi_lo - ((int64_t) ((int32_t) get(i.get_rs())) *
                                     (int64_t) ((int32_t) get(i.get_rt()))));
                  break;
    case IC_MSUBU: set_hi_lo(hi_lo - ((uint64_t) get(i.get_rs()) *
                                      (uint64_t) get(i.get_rt())));
                   break;
    case IC_MSUB_D: unimplemented_instr(i, pc);
    case IC_MSUB_PS: unimplemented_instr(i, pc);
    case IC_MSUB_S: unimplemented_instr(i, pc);
    case IC_MTC0: unimplemented_instr(i, pc);
    case IC_MTC1: unimplemented_instr(i, pc);
    case IC_MTC2: unimplemented_instr(i, pc);
    case IC_MTHC1: unimplemented_instr(i, pc);
    case IC_MTHC2: unimplemented_instr(i, pc);
    case IC_MTHI: set_hi_lo((hi_lo & 0xffffffffU)
                            | (((uint64_t) get(i.get_rs())) << 32));
                  break;
    case IC_MTLO: set_hi_lo((hi_lo & 0xffffffff00000000ULL)
                            | get(i.get_rs()));
                  break;
    case IC_MUL: set(i.get_rd(),
                     (int32_t) get(i.get_rs()) * (int32_t) get(i.get_rt()));
                 break;
    case IC_MULT: set_hi_lo((int64_t) ((int32_t) get(i.get_rs())) *
                            (int64_t) ((int32_t) get(i.get_rt())));
                  break;
    case IC_MULTU: set_hi_lo((uint64_t) get(i.get_rs()) *
                             (uint64_t) get(i.get_rt()));
                   break;
    case IC_MUL_D: unimplemented_instr(i, pc);
    case IC_MUL_PS: unimplemented_instr(i, pc);
    case IC_MUL_S: unimplemented_instr(i, pc);
    case IC_NEG_D: unimplemented_instr(i, pc);
    case IC_NEG_PS: unimplemented_instr(i, pc);
    case IC_NEG_S: unimplemented_instr(i, pc);
    case IC_NMADD_D: unimplemented_instr(i, pc);
    case IC_NMADD_PS: unimplemented_instr(i, pc);
    case IC_NMADD_S: unimplemented_instr(i, pc);
    case IC_NMSUB_D: unimplemented_instr(i, pc);
    case IC_NMSUB_PS: unimplemented_instr(i, pc);
    case IC_NMSUB_S: unimplemented_instr(i, pc);
    case IC_NOP: break;
    case IC_NOR: set(i.get_rd(), ~(get(i.get_rs()) | get(i.get_rt()))); break;
    case IC_OR: op3ur(|); break;
    case IC_ORI: op3ui(|); break;
    case IC_PAUSE: unimplemented_instr(i, pc);
    case IC_PLL_PS: unimplemented_instr(i, pc);
    case IC_PLU_PS: unimplemented_instr(i, pc);
    case IC_PREF: unimplemented_instr(i, pc);
    case IC_PREFX: unimplemented_instr(i, pc);
    case IC_PUL_PS: unimplemented_instr(i, pc);
    case IC_PUU_PS: unimplemented_instr(i, pc);
    case IC_RDHWR: set(i.get_rt(), get(i.get_hwrd())); break;
    case IC_RDPGPR: unimplemented_instr(i, pc);
    case IC_RECIP_D: unimplemented_instr(i, pc);
    case IC_RECIP_S: unimplemented_instr(i, pc);
    case IC_ROTR: unimplemented_instr(i, pc); // XXX
    case IC_ROTRV: unimplemented_instr(i, pc); // XXX
    case IC_ROUND_L_D: unimplemented_instr(i, pc);
    case IC_ROUND_L_S: unimplemented_instr(i, pc);
    case IC_ROUND_W_D: unimplemented_instr(i, pc);
    case IC_ROUND_W_S: unimplemented_instr(i, pc);
    case IC_RSQRT_D: unimplemented_instr(i, pc);
    case IC_RSQRT_S: unimplemented_instr(i, pc);
    case IC_SB: store(mem_addr(), (uint8_t) get(i.get_rt())); break;
    case IC_SC: unimplemented_instr(i, pc);
    case IC_SDBBP: unimplemented_instr(i, pc);
    case IC_SDC1: unimplemented_instr(i, pc);
    case IC_SDC2: unimplemented_instr(i, pc);
    case IC_SDXC1: unimplemented_instr(i, pc);
    case IC_SEB: set(i.get_rd(), (int32_t) ((int8_t) get(i.get_rt()))); break;
    case IC_SEH: set(i.get_rd(), (int32_t) ((int16_t) get(i.get_rt()))); break;
    case IC_SH: store(mem_addrh(), (uint16_t) get(i.get_rt())); break;
    case IC_SLL: set(i.get_rd(), get(i.get_rt()) << i.get_sa()); break;
    case IC_SLLV: set(i.get_rd(), get(i.get_rt()) << get(i.get_rs())); break;
    case IC_SLT: op3sr(<); break;
    case IC_SLTI: op3si(<); break;
    case IC_SLTIU: op3ui(<); break;
    case IC_SLTU: op3ur(<); break;
    case IC_SQRT_D: unimplemented_instr(i, pc);
    case IC_SQRT_S: unimplemented_instr(i, pc);
    case IC_SRA: set(i.get_rd(), ((int32_t) get(i.get_rt())) >> i.get_sa());
                 break;
    case IC_SRAV: set(i.get_rd(),
                      ((int32_t) get(i.get_rt())) >> get(i.get_rs()));
                 break;
    case IC_SRL: set(i.get_rd(), get(i.get_rt()) >> i.get_sa()); break;
    case IC_SRLV: set(i.get_rd(), get(i.get_rt()) >> get(i.get_rs())); break;
    case IC_SSNOP: break;
    case IC_SUB: op3ur_of(-); break;
    case IC_SUBU: op3ur(-); break;
    case IC_SUB_D: unimplemented_instr(i, pc);
    case IC_SUB_PS: unimplemented_instr(i, pc);
    case IC_SUB_S: unimplemented_instr(i, pc);
    case IC_SUXC1: unimplemented_instr(i, pc);
    case IC_SW: store(mem_addrw(), get(i.get_rt())); break;
    case IC_SWC1: unimplemented_instr(i, pc);
    case IC_SWC2: unimplemented_instr(i, pc);
    case IC_SWL: unimplemented_instr(i, pc);
    case IC_SWR: unimplemented_instr(i, pc);
    case IC_SWXC1: unimplemented_instr(i, pc);
    case IC_SYNC: unimplemented_instr(i, pc);
    case IC_SYNCI: unimplemented_instr(i, pc);
    case IC_SYSCALL: syscall(get(gpr(2))); break;
    case IC_TEQ: unimplemented_instr(i, pc);
    case IC_TEQI: unimplemented_instr(i, pc);
    case IC_TGE: unimplemented_instr(i, pc);
    case IC_TGEI: unimplemented_instr(i, pc);
    case IC_TGEIU: unimplemented_instr(i, pc);
    case IC_TGEU: unimplemented_instr(i, pc);
    case IC_TLBP: unimplemented_instr(i, pc);
    case IC_TLBR: unimplemented_instr(i, pc);
    case IC_TLBWI: unimplemented_instr(i, pc);
    case IC_TLBWR: unimplemented_instr(i, pc);
    case IC_TLT: unimplemented_instr(i, pc);
    case IC_TLTI: unimplemented_instr(i, pc);
    case IC_TLTIU: unimplemented_instr(i, pc);
    case IC_TLTU: unimplemented_instr(i, pc);
    case IC_TNE: unimplemented_instr(i, pc);
    case IC_TNEI: unimplemented_instr(i, pc);
    case IC_TRUNC_L_D: unimplemented_instr(i, pc);
    case IC_TRUNC_L_S: unimplemented_instr(i, pc);
    case IC_TRUNC_W_D: unimplemented_instr(i, pc);
    case IC_TRUNC_W_S: unimplemented_instr(i, pc);
    case IC_WAIT: unimplemented_instr(i, pc);
    case IC_WRPGPR: unimplemented_instr(i, pc);
    case IC_WSBH: unimplemented_instr(i, pc);
    case IC_XOR: op3ur(^); break;
    case IC_XORI: op3ui(^); break;
    default: unimplemented_instr(i, pc);
    }
}

