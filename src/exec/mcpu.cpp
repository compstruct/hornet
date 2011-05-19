// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <cstring>
#include "endian.hpp"
#include "syscalls.hpp"
#include "mcpu.hpp"
#include "math.h"
#include "dramController.hpp"

using namespace std;

mcpu::mcpu( const pe_id                         &new_id, 
            const uint64_t                      &new_time,
            uint32_t                            entry_point, 
            uint32_t                            stack_ptr,
            shared_ptr<id_factory<packet_id> >  pif,
            shared_ptr<tile_statistics>         new_stats,
            logger                              &l,
            shared_ptr<random_gen>              r,
            shared_ptr<memory>                  instruction_memory,
            shared_ptr<memory>                  data_memory,
            shared_ptr<dram>                    backingDRAM,
            uint32_t                            msg_queue_size, 
            uint32_t                            bytes_per_flit)
            throw(err)
            :   core(                           new_id, 
                                                new_time, 
                                                pif,
                                                new_stats,
                                                l, 
                                                r, 
                                                data_memory, 
                                                0,
                                                msg_queue_size, 
                                                bytes_per_flit), // TODO: the 0 means no core support for EM2 
                running(true), 
                instr_count(0),
                byte_count(4), // TODO make static---move to mcpu.hpp
                pc(entry_point), 
                net(),
                jump_active(false), 
                interrupts_enabled(false), 
                stdout_buffer(),
                i_memory(instruction_memory),
                pending_request_instruction(shared_ptr<memoryRequest>()),
                pending_request_data(shared_ptr<memoryRequest>()),
                pending_request_read_gpr(false),
                pending_request_read_fpr(false),
                pending_request_memory_write(false),
                pending_lw_gpr(gpr(0)),
                pending_lw_fpr(fpr(0)),
                backingDRAM(backingDRAM),               
                fid_key(0),
                fid_value(NULL) {
    pc = entry_point;
    gprs[29] = stack_ptr;
    cout     << "mcpu " << get_id() << " created with entry point at "
             << hex << setfill('0') << setw(8) << pc << " and stack pointer at "
             << setw(8) << stack_ptr << endl;
}

mcpu::~mcpu() throw() { }

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

/* 0 =  global address space, 
        thread local addresses given by thread ID + 1 */
maddr_t mcpu::form_maddr(uint64_t addr) {
    maddr_t maddr;
    int aspace = addr & 0x00400000; 
    maddr.space = (aspace) ? get_id().get_numeric_id() + 1 : 0;
    maddr.address = addr;
    return maddr;
}

/* These functions should perhaps be moved elsewhere. */

float intBitsToFloat(int x) {
	union {
		float f;  // assuming 32-bit IEEE 754 single-precision
		int i;    // assuming 32-bit 2's complement int
	} u;
	u.i = x;
	return u.f;
}

double intBitsToDouble(uint64_t x) {
	union {
		double f;  			// assuming 64-bit IEEE 754 double-precision
		uint64_t i;    	// assuming 64-bit 2's complement int
	} u;
	u.i = x;
	return u.f;
}

uint32_t floatBitsToInt(float x) {
    uint32_t y;
    memcpy(&y, &x, 4);
    return y;
}

uint64_t doubleBitsToInt(double x) {
    uint64_t y;
    memcpy(&y, &x, 8);
    return y;
}

// TODO: Copied from sys.cpp
inline uint32_t read_word_temp(shared_ptr<ifstream> in) throw(err) {
    uint32_t word = 0xdeadbeef;
    in->read((char *) &word, 4);
    if (in->bad()) throw err_bad_mem_img();
    word = endian(word);
    return word;
}

/* -------------------------------------------------------------------------- */
/* Memory hierarchy                                                           */
/* -------------------------------------------------------------------------- */

void mcpu::update_from_memory_requests() {
    /* update waiting requests */
}

void mcpu::tick_positive_edge_memory() throw(err) {
    if (m_memory != shared_ptr<memory>()) {
        m_memory->tick_positive_edge();
    }
    /*if (i_memory != shared_ptr<memory>()) {
        i_memory->tick_positive_edge();
    }*/
}
void mcpu::tick_negative_edge_memory() throw(err) {
    /* update requests */
    update_from_memory_requests();

    if (m_memory != shared_ptr<memory>()) {
        m_memory->tick_negative_edge();
    }
    /*if (i_memory != shared_ptr<memory>()) {
        i_memory->tick_negative_edge();
    }*/
}

/* -------------------------------------------------------------------------- */
/* Core interface                                                             */
/* -------------------------------------------------------------------------- */

uint64_t mcpu::next_pkt_time() throw(err) {
    if (running) {
        return system_time;
    } else {
        return UINT64_MAX;
    }
}

bool mcpu::is_drained() const throw() {
    return !running;
}

void mcpu::flush_stdout() throw() {
    if (!stdout_buffer.str().empty()) {
        LOG(log,0) << "[mcpu " << get_id() << " out] " << stdout_buffer.str()
                   << flush;
        stdout_buffer.str("");
    }
}

void mcpu::execute() {
    if (running) { 
        //cout << "exec_core PID: " << get_id() << ", jump_active: " << jump_active << endl;
        data_complete();
        shared_ptr<instr> i = instruction_fetch_complete(pc);
        printf("[mcpu %d] Running: %d\n", get_id().get_numeric_id(), running);
        if (i && !pending_data_memory_request()) {
            instr_count++;
            execute(i);
            if (jump_active && (jump_time <= system_time)) {
                if (get_id() == 0) 
                      LOG(log,5) << "[cpu " << get_id() << "]   pc <- "
                           << hex << setfill('0') << setw(8) << jump_target << endl;
                pc = jump_target;
                jump_active = false;
            } else { pc += 4; }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Memory interface                                                           */
/* -------------------------------------------------------------------------- */

// Instructions ----------------------------------------------------------------

shared_ptr<instr> mcpu::instruction_fetch_complete(uint32_t pc) {
    if (!pending_data_memory_request() &&
        pending_request_instruction && 
        pending_request_instruction->status() == REQ_DONE) {
        uint32_t raw = pending_request_instruction->data()[0];
        printf( "[mcpu %d] Completed instruction_fetch_complete, address: %x, instr: %x\n", 
                get_id().get_numeric_id(), pc, raw);
        pending_request_instruction = shared_ptr<memoryRequest>(); // reset to null
        return shared_ptr<instr> (new instr(raw));
    }
    if (!pending_data_memory_request() && // TODO: serialize data/instr for now (fix later)
        !pending_request_instruction) {
        maddr_t maddr = form_maddr(pc);
        printf("--------------------------------------------------------------------\n");
        printf( "[mcpu %d] Issued instruction_fetch_complete, address: %x (Hornet word address: %x)\n", 
                get_id().get_numeric_id(), pc, (uint32_t) maddr.address);
        shared_ptr<memoryRequest> read_req(new memoryRequest(maddr, 1));
        nearest_memory_instruction()->request(read_req);
        pending_request_instruction = shared_ptr<memoryRequest>(read_req);
    }
    return shared_ptr<instr>();
}

// Data complete ---------------------------------------------------------------

void mcpu::data_complete() {
    if (!pending_request_instruction &&  // TODO: serialize data/instr for now (fix later)
        pending_data_memory_request() &&
        pending_request_data->status() == REQ_DONE) {
        if (pending_request_data->is_read()) data_complete_helper(pending_request_data);
        close_memory_op();
    }
}

void mcpu::data_complete_helper(const shared_ptr<memoryRequest> &req) {
    uint64_t m;
    switch (pending_request_byte_count) { 
        case 1: m = 0xFFULL; break;  
        case 2: m = 0xFFFFULL; break;
        case 3: m = 0xFFFFFFULL; break;
        case 4: m = 0xFFFFFFFFULL; break;
        case 8: m = 0xFFFFFFFFFFFFFFFFULL; break;
        default: throw exc_addr_align();
    }
    uint64_t raw;
    if (pending_request_byte_count == 8) {
        // this order corresponds to what is written in the .o file
        uint64_t bot = req->data()[1]; 
        uint64_t top = ((uint64_t) req->data()[0]) << 32;
        raw = bot | top;
    } else {
        raw = req->data()[0];
    }
    raw = raw & m;
    if (pending_lw_sign_extend) {
        uint32_t check = raw & (1 << (pending_request_byte_count*8 - 1));
        m = (check) ? 0xffffffffffffffffULL << (pending_request_byte_count*8) : 0x0;
        raw = raw | m;
    }
    if (pending_request_read_gpr) {
        //if (get_id() == 0) 
        //    printf("[mcpu %d] Completed load gpr[%d], address: %x, data: %016llX\n", 
        //            get_id().get_numeric_id(), pending_lw_gpr.get_no(), (uint32_t) req->maddr().address, 
        //            (long long unsigned int) raw); 
        set(gpr(pending_lw_gpr), raw);
    } else {
        assert(pending_request_read_fpr);
        //if (get_id() == 0) 
        //    printf("[mcpu %d] Completed load fpr[%d], address: %x, data: %f (%016llX)\n", 
        //            get_id().get_numeric_id(), pending_lw_fpr.get_no(), (uint32_t) req->maddr().address,
        //            intBitsToDouble(raw), (long long unsigned int) raw);            
        if (pending_request_byte_count == 4) set_s(fpr(pending_lw_fpr), raw);
        else if (pending_request_byte_count == 8) set_d(fpr(pending_lw_fpr), raw);
        else throw exc_addr_align();
    }
}

// Data fetch ------------------------------------------------------------------

#define __BYTES_TO_WORDS__      uint32_t words; \
                                if (bytes <= 4) words = 1; \
                                else if (bytes <= 8) words = 2; \
                                else assert(false);

void mcpu::data_fetch_to_gpr(   const gpr dst,
                                const uint32_t &addr, 
                                const uint32_t &bytes,
                                bool sign_extend) throw(err) {
    data_fetch_read(addr, bytes, sign_extend);
    pending_lw_gpr = dst;
    pending_request_read_gpr = true;
}
void mcpu::data_fetch_to_fpr(   const fpr dst,
                                const uint32_t &addr, 
                                const uint32_t &bytes) throw(err) {
    data_fetch_read(addr, bytes, false);
    pending_lw_fpr = dst;
    pending_request_read_fpr = true;
}
void mcpu::data_fetch_read( const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err) {
    assert(!pending_data_memory_request());
    //printf("--------------------------------------------------------------------\n");
    //printf("[mcpu %d] Memory read at %x\n", get_id().get_numeric_id(), addr);

    __BYTES_TO_WORDS__

    pending_request_byte_count = bytes;
    pending_request_data = shared_ptr<memoryRequest> (new memoryRequest(form_maddr(addr), words));
    nearest_memory_data()->request(pending_request_data);
    pending_lw_sign_extend = sign_extend;
}

void mcpu::data_fetch_write(    const uint32_t &addr, 
                                const uint64_t &val,
                                const uint32_t &bytes) throw(err) {
    assert(!pending_data_memory_request());
    
    __BYTES_TO_WORDS__

    //printf("--------------------------------------------------------------------\n");
    shared_array<uint32_t> wdata(new uint32_t[words]);
    if (bytes != 8) {
        assert(bytes <= 4);
        wdata[0] = (uint32_t) val;
        //printf( "[mcpu %d] Memory write at %x, data: %x\n", 
        //        get_id().get_numeric_id(), addr, wdata[0]);
    } else {
        wdata[0] = (uint32_t) (val >> 32);
        wdata[1] = (uint32_t) val;
        //printf( "[mcpu %d] Memory write at %x, data: {%x, %x} \n", 
        //        get_id().get_numeric_id(), addr, wdata[1], wdata[0]);
    }

    pending_request_data = shared_ptr<memoryRequest> (new memoryRequest(form_maddr(addr), words, wdata));
    nearest_memory_data()->request(pending_request_data);
    pending_request_memory_write = true;
}

/* -------------------------------------------------------------------------- */
/* MIPS scalar                                                                */
/* -------------------------------------------------------------------------- */

static void unimplemented_instr(instr i, uint32_t addr) throw(err_tbd) {
    ostringstream oss;
    oss << "[0x" << hex << setfill('0') << setw(8) << addr << "] " << i;
    throw err_tbd(oss.str());
}

inline uint32_t check_align(uint32_t addr, uint32_t mask) {
    if (addr & mask) throw exc_addr_align();
    return addr;
}

// Scalar
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
#define branch() { jump_active = true; jump_time = system_time + 1; \
                   jump_target = pc + 4 + (i.get_simm() << 2); }
#define jump(addr) { jump_active = true; jump_time = system_time + 1; \
                     jump_target = addr; }
#define branch_link() { set(gpr(31), pc + 8); branch(); }
#define branch_now() { pc += (i.get_simm() << 2); }
#define branch_link_now() { set(gpr(31), pc + 8); branch_now(); }
#define mem_addr() (get(i.get_rs()) + i.get_simm())
#define mem_addrh() (check_align(mem_addr(), 0x1))
#define mem_addrw() (check_align(mem_addr(), 0x3))

// Floating point
#define opcfp_s(n, op) {    float left = intBitsToFloat(get_s(i.get_fs())); \
                            float right = intBitsToFloat(get_s(i.get_ft())); \
                            bool result = left op right; \
                            set_cp1_cf(cfr(n), result); }
#define op3fp_s(op) {       float left = intBitsToFloat(get_s(i.get_fs())); \
                            float right = intBitsToFloat(get_s(i.get_ft())); \
                            float result = left op right; \
                            set_s(i.get_fd(), floatBitsToInt(result)); }
#define op3fp_d(op) {       double left = intBitsToDouble(get_d(i.get_fs())); \
                            double right = intBitsToDouble(get_d(i.get_ft())); \
                            double result = left op right; \
                            set_d(i.get_fd(), doubleBitsToInt(result)); }

// Syscall
#define __prefix_single__   float in = intBitsToFloat(get(gpr(4)));
#define __suffix_single__   set(gpr(2), floatBitsToInt(result)); break;
#define __prefix_double__   uint32_t bot_i = get(gpr(4)); \
                            uint64_t top_i = get(gpr(5)); \
                            uint64_t com = (top_i << 32) | bot_i; \
                            double in = intBitsToDouble(com);
#define __suffix_double__   uint64_t out = doubleBitsToInt(result); \
                            uint32_t bot_o = out; \
                            uint32_t top_o = out >> 32; \
                            set(gpr(2), bot_o); \
                            set(gpr(3), top_o);

void mcpu::execute(shared_ptr<instr> ip) throw(err) {
    instr i = *ip;
    cout << "[mcpu " << get_id() << "] "
         << hex << setfill('0') << setw(8) << pc << ": "
         << i << " (instr #: " << instr_count << ")" << endl;
    instr_code code = i.get_opcode();
    switch (code) {
    case IC_ABS_D: unimplemented_instr(i, pc);
    case IC_ABS_PS: unimplemented_instr(i, pc);
    case IC_ABS_S: unimplemented_instr(i, pc);
    case IC_ADD: op3ur_of(+); break;
    case IC_ADDI: op3si_of(+); break;
    case IC_ADDIU: op3si(+); break;
    case IC_ADDU: op3ur(+); break;
    case IC_ADD_D: // FP
        op3fp_d(+);
        break;
    case IC_ADD_PS: unimplemented_instr(i, pc);
    case IC_ADD_S: // FP
        op3fp_s(+);
        break;
    case IC_ALNV_PS: unimplemented_instr(i, pc);
    case IC_AND: op3ur(&); break;
    case IC_ANDI: op3ui(&); break;
    case IC_B: branch(); break;
    case IC_BAL: branch_link(); break;
    case IC_BC1F: unimplemented_instr(i, pc);
    case IC_BC1FL: unimplemented_instr(i, pc);
    case IC_BC1T: // FP
        if (get_cp1_cf(cfr(0))) branch(); 
        break;
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
    case IC_CVT_D_S: {
        double src = (double) intBitsToFloat(get_s(i.get_fs()));
        set_d(i.get_fd(), doubleBitsToInt(src)); 
        break;        
    }
    case IC_CVT_D_W: unimplemented_instr(i, pc);
    case IC_CVT_L_D: unimplemented_instr(i, pc);
    case IC_CVT_L_S: unimplemented_instr(i, pc);
    case IC_CVT_PS_S: unimplemented_instr(i, pc);
    case IC_CVT_S_D: { // FP
        float src = (float) intBitsToDouble(get_d(i.get_fs()));
        set_s(i.get_fd(), floatBitsToInt(src)); 
        break;    
    }
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
    case IC_C_LT_S: // FP
        opcfp_s(0, <);
        break;
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
    case IC_DIV_D: // FP
        op3fp_d(/);
        break;        
    case IC_DIV_S: // FP
        op3fp_s(/);
        break;
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
    case IC_LB: data_fetch_to_gpr(i.get_rt(), mem_addr(), 1, true); break;
    case IC_LBU: data_fetch_to_gpr(i.get_rt(), mem_addr(), 1, false); break;
    case IC_LDC1: // FP
        data_fetch_to_fpr(i.get_ft(), mem_addrw(), 8);
        break;
    case IC_LDC2: unimplemented_instr(i, pc);
    case IC_LDXC1: unimplemented_instr(i, pc);
    case IC_LH: data_fetch_to_gpr(i.get_rt(), mem_addrh(), 2, true); break;
    case IC_LHU: data_fetch_to_gpr(i.get_rt(), mem_addrh(), 2, false); break;
    case IC_LL: unimplemented_instr(i, pc);
    case IC_LUI: set(i.get_rt(), i.get_imm() << 16); break;
    case IC_LUXC1: unimplemented_instr(i, pc);
    case IC_LW: data_fetch_to_gpr(i.get_rt(), mem_addrw(), 4, true); break;
    case IC_LWC1: // FP
        data_fetch_to_fpr(i.get_ft(), mem_addrw(), 4);
        break;
    case IC_LWC2: unimplemented_instr(i, pc);
    case IC_LWL: unimplemented_instr(i, pc);
    /*{ TODO: do we actually need these?
        uint32_t a = mem_addr();
        uint32_t w = load<uint32_t>(a);
        uint32_t m = 0xffffffffU << ((a & 0x3) << 3);
        set(i.get_rt(), combine(get(i.get_rt()), w, m));
        break;
    }*/
    case IC_LWR: unimplemented_instr(i, pc);
    /*{ TODO: do we actually need these?
        uint32_t a = mem_addr();
        uint32_t w = load<uint32_t>(a - 3);
        uint32_t m = ~(0xffffff00U << ((a & 0x3) << 3));
        set(i.get_rt(), combine(get(i.get_rt()), w, m));
        break;
    }*/
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
    case IC_MFC1: // FP
        set(i.get_rt(), get_s(i.get_fs())); 
        break;
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
    case IC_MOV_D: // FP
        set_d(i.get_fd(), get_d(i.get_fs())); 
        break;        
    case IC_MOV_PS: unimplemented_instr(i, pc);
    case IC_MOV_S: // FP
        set_s(i.get_fd(), get_s(i.get_fs())); 
        break;
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
    case IC_MTC1: // FP
        set_s(i.get_fs(), get(i.get_rt())); 
        break;
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
    case IC_MUL_D: // FP
        op3fp_d(*);
        break;
    case IC_MUL_PS: unimplemented_instr(i, pc);
    case IC_MUL_S: // FP
        op3fp_s(*);
        break;
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
    case IC_SB: data_fetch_write(mem_addr(), get(i.get_rt()), 1); break;
    case IC_SC: unimplemented_instr(i, pc);
    case IC_SDBBP: unimplemented_instr(i, pc);
    case IC_SDC1: // FP
        data_fetch_write(mem_addrw(), get_d(i.get_ft()), 8); 
        break;
    case IC_SDC2: unimplemented_instr(i, pc);
    case IC_SDXC1: unimplemented_instr(i, pc);
    case IC_SEB: set(i.get_rd(), (int32_t) ((int8_t) get(i.get_rt()))); break;
    case IC_SEH: set(i.get_rd(), (int32_t) ((int16_t) get(i.get_rt()))); break;
    case IC_SH: data_fetch_write(mem_addrh(), get(i.get_rt()), 2); break;
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
    case IC_SUB_D: 
        op3fp_d(-);
        break;
    case IC_SUB_PS: unimplemented_instr(i, pc);
    case IC_SUB_S: // FP
        op3fp_s(-);
        break;
    case IC_SUXC1: unimplemented_instr(i, pc);
    case IC_SW: data_fetch_write(mem_addrw(), get(i.get_rt()), 4); break;
    case IC_SWC1: 
        data_fetch_write(mem_addrw(), get_d(i.get_ft()), 4); 
        break;
    case IC_SWC2: unimplemented_instr(i, pc);
    case IC_SWL: unimplemented_instr(i, pc);
    case IC_SWR: unimplemented_instr(i, pc);
    case IC_SWXC1: unimplemented_instr(i, pc);
    case IC_SYNC: unimplemented_instr(i, pc);
    case IC_SYNCI: unimplemented_instr(i, pc);
    case IC_SYSCALL: syscall(get(gpr(2))); break;
    case IC_TEQ: 
        if (get(i.get_rs()) == get(i.get_rt())) trap();
        break;
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
    case IC_TRUNC_W_S:
        set_s(i.get_fd(), ((int32_t) get_s(i.get_fs())));
        break;
    case IC_WAIT: unimplemented_instr(i, pc);
    case IC_WRPGPR: unimplemented_instr(i, pc);
    case IC_WSBH: unimplemented_instr(i, pc);
    case IC_XOR: op3ur(^); break;
    case IC_XORI: op3ui(^); break;
    default: unimplemented_instr(i, pc);
    }
}

void mcpu::syscall(uint32_t call_no) throw(err) {
    switch (call_no) {
    // Single precision intrinsics ---------------------------------------------
    case SYSCALL_SQRT_S: {
        __prefix_single__
        float result = sqrt(in);
        __suffix_single__
    }
    case SYSCALL_LOG_S: {
        __prefix_single__
        float result = std::log(in);
        __suffix_single__
    }
    case SYSCALL_EXP_S: {
        __prefix_single__
        float result = exp(in);
        __suffix_single__
    }
    // Double precision intrinsics ---------------------------------------------
    case SYSCALL_SQRT_D: {
        __prefix_double__
        double result = sqrt(in);
        __suffix_double__
        break;
    }
    case SYSCALL_LOG_D: {
        __prefix_double__
        double result = std::log(in);
        __suffix_double__
        break;
    }
    case SYSCALL_EXP_D: {
        __prefix_double__
        double result = exp(in);
        __suffix_double__
        break;
    }
    // File I/O ----------------------------------------------------------------
    case SYSCALL_FOPEN: {
        // 1.) build file name from memory
        /*maddr_t fname_start = form_maddr(get(gpr(4)));
        uint32_t * fname_buffer = (uint32_t *) malloc(sizeof(uint32_t) * MAX_BUFFER_SIZE);
        backingDRAM->mem_read_instant(  fname_buffer, fname_start,
                                        MAX_BUFFER_SIZE, true);
        assert(false);
        // 2.) open the file & return a key
        fid_value = fopen(fname_buffer, "r");
        fid_key = (fid_value == NULL) ? 0 : 1; // TODO: turn into map for multi-file support
        free(fname_buffer);
        set(gpr(2), fid_key);*/
        break;
    }
    case SYSCALL_READ_LINE: {
        /*// 1.) inputs
        int fid_key_temp = get(gpr(4));
        if (fid_key_temp == -1 || fid_key_temp != fid_key)
            err_panic("MCPU: SYSCALL_FSCANF BAD FILE.");      
        uint32_t dest = (uint32_t) get(gpr(5));
        uint32_t count = (uint32_t) get(gpr(6));
        uint32_t size = sizeof(char) * MAX_BUFFER_SIZE;
        // 2.) call
        char * walk; char * data_buffer;
        data_buffer = (char *) malloc(size);  walk = data_buffer; 
        char lc = NULL; uint32_t write_count;
        for (write_count = 0; write_count < count; write_count++) {
            if (feof(fid_value)) break;
            lc = fgetc(fid_value);
            //if (get_id() == 0)
            //    printf("Just read: %1x\n", (unsigned)(unsigned char) lc);
            *walk = lc; walk++;
        }
        backingDRAM->mem_write_instant(get_id().get_numeric_id(), 
                                            data_buffer, dest, 
                                            write_count, false);
        free(data_buffer);
        set(gpr(2), write_count);*/
        assert(false);
        break;
    }
    case SYSCALL_FCLOSE: {
        int fid_key_temp = get(gpr(4));
        if (fid_key_temp == -1 || fid_key_temp != fid_key)
            err_panic("MCPU: SYSCALL_FSCANF BAD FILE.");
        int ret = fclose(fid_value);
        set(gpr(2), ret);    
        break;
    }
    // Unached LW/SW -----------------------------------------------------------
    /*case SYSCALL_UNCACHED_LOAD_WORD: {
        maddr_t addr = form_maddr(get(gpr(4)));
        int loaded;
        backingDRAM->mem_read_instant( get_id().get_numeric_id(), 
                                            &loaded, addr, 4, true);
        set(gpr(2), loaded);
        break;
    }
    case SYSCALL_UNCACHED_SET_BIT: {
        maddr_t addr = (maddr_t) get(gpr(4));
        uint32_t position = get(gpr(5));
        uint32_t mask = 0x1 << position;
        int loaded;
        backingDRAM->mem_read_instant( get_id().get_numeric_id(), 
                                            &loaded, addr, 4, true);
        loaded = loaded | mask;
        backingDRAM->mem_write_instant(get_id().get_numeric_id(), 
                                            &loaded, addr, 4, true);
        break;    
    }*/
    // Printers ----------------------------------------------------------------
    case SYSCALL_PRINT_CHAR: {
        char p = (char) get(gpr(4));
        stdout_buffer << dec << p; 
        break;
    }
    case SYSCALL_PRINT_INT: {
        int p = (int32_t) get(gpr(4));
        stdout_buffer << dec << p;
        break;
    }
    case SYSCALL_PRINT_FLOAT: {
        float p = intBitsToFloat(get(gpr(4)));
        stdout_buffer << dec << p; 
        break;
    }
    case SYSCALL_PRINT_DOUBLE: {
        __prefix_double__
        stdout_buffer << dec << in; 
        break;
    }
    case SYSCALL_PRINT_STRING: {
        maddr_t fname_maddr = form_maddr(get(gpr(4)));
        uint32_t * fname_buffer = (uint32_t *) malloc(sizeof(uint32_t) * MAX_BUFFER_SIZE);
        backingDRAM->mem_read_instant(  fname_buffer, 
                                        fname_maddr.space, fname_maddr.address, 
                                        MAX_BUFFER_SIZE, true);
        stdout_buffer << ((char *) fname_buffer);
        free(fname_buffer);
        break;
    }
    case SYSCALL_FLUSH: {
        flush_stdout();
        break;
    }
    // Exits -------------------------------------------------------------------
    case SYSCALL_EXIT_SUCCESS:
        flush_stdout(); 
        printf("CPU %d exited successfully!\n", get_id().get_numeric_id());
        running = false; break;
    case SYSCALL_EXIT: {
        int code = get(gpr(4));
        flush_stdout();
        printf("CPU %d exited!\n", get_id().get_numeric_id());
        running = false;
        exit(code);
    }
    case SYSCALL_ASSERT: {
        int r = get(gpr(4));
        if (!r) {
            flush_stdout();
            running = false;
            err_panic("Assertion failure.");
        }  
        break;      
    }
    // Network -----------------------------------------------------------------
    case SYSCALL_SEND:
        err_panic("MCPU: SYSCALL_SEND not implemented.");
        /* TODO
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->send(get(gpr(4)), ram->ptr(get(gpr(5))),
                              ((get(gpr(6)) >> 3) +
                               ((get(gpr(6)) & 0x7) != 0 ? 1 : 0)),
                               stats->is_started()));
        */        
        break;
    case SYSCALL_RECEIVE:
        err_panic("MCPU: SYSCALL_RECEIVE not implemented.");
        /*
        if (!net) throw exc_no_network(get_id().get_numeric_id());
        set(gpr(2), net->receive(ram->ptr(get(gpr(5))), get(gpr(4)),
                                 get(gpr(6)) >> 3));
        */
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

void mcpu::trap() throw(err) {
    err_panic("Trap raised!");
}

