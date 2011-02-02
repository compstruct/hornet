// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include "endian.hpp"
#include "syscalls.hpp"
#include "mcpu.hpp"

using namespace std;

mcpu::mcpu( const pe_id                         &new_id, 
            const uint64_t                      &new_time,
            uint32_t                            entry_point, 
            uint32_t                            stack_ptr,
            shared_ptr<id_factory<packet_id> >  pif,
            shared_ptr<tile_statistics>         new_stats,
            logger                              &l,
            shared_ptr<random_gen>              r,
            core_cfg_t                          core_cfgs) 
            throw(err)
            :   core(                           new_id, 
                                                new_time, 
                                                pif,
                                                new_stats,
                                                l, 
                                                r, 
                                                core_cfgs), 
                running(true), 
                instr_count(0),
                byte_count(4),
                pc(entry_point), 
                net(),
                jump_active(false), 
                interrupts_enabled(false), 
                stdout_buffer(),
                pending_i_request(false),
                pending_request(false),
                pending_lw_gpr(gpr(0)) { // TODO: don't pass by copy
    pc = entry_point;
    gprs[29] = stack_ptr;
    LOG(log,3) << "mcpu " << get_id() << " created with entry point at "
             << hex << setfill('0') << setw(8) << pc << " and stack pointer at "
             << setw(8) << stack_ptr << endl;
}

mcpu::~mcpu() throw() { }

/* -------------------------------------------------------------------------- */
/* Memory hierarchy                                                           */
/* -------------------------------------------------------------------------- */

// TODO: Copied from sys.cpp
inline uint32_t read_word_temp(shared_ptr<ifstream> in) throw(err) {
    uint32_t word = 0xdeadbeef;
    in->read((char *) &word, 4);
    if (in->bad()) throw err_bad_mem_img();
    word = endian(word);
    return word;
}

void mcpu::add_to_i_memory_hierarchy(int level, shared_ptr<memory> mem) {
    /* assume only one meory per level */
    assert(m_i_memory_hierarchy.count(level) == 0);
    if (m_i_memory_hierarchy.size() == 0 || level < m_i_min_memory_level) {
        m_i_min_memory_level = level;
    }
    if (level > m_i_max_memory_level) {
        m_i_max_memory_level = level;
    }
    m_i_memory_hierarchy[level] = mem;
}

void mcpu::mh_initiate() {
    core::mh_initiate();
    for (int level = 0; level <= m_i_max_memory_level; ++level) {
        if (m_i_memory_hierarchy.count(level)) {
            m_i_memory_hierarchy[level]->initiate();
        }
    }
}

void mcpu::mh_update() {
    core::mh_update();
    for (int level = 0; level <= m_i_max_memory_level; ++level) {
        if (m_i_memory_hierarchy.count(level)) {
            m_i_memory_hierarchy[level]->update();
        }
    }    
}

void mcpu::mh_process() {
    core::mh_process();
    for (int level = 0; level <= m_i_max_memory_level; ++level) {
        if (m_i_memory_hierarchy.count(level)) {
            m_i_memory_hierarchy[level]->process();
        }
    }
}

void mcpu::initialize_memory_hierarchy( uint32_t id,
                                        shared_ptr<tile> tile,
                                        shared_ptr<ifstream> img, 
                                        bool icash,
                                        shared_ptr<remoteMemory> rm,
                                        uint32_t mem_start,
                                        uint32_t mem_size, 
                                        shared_ptr<mem> m) {

    shared_ptr<cache> last_local_cache = shared_ptr<cache>();
    shared_ptr<dram> new_dram = shared_ptr<dram>();

    uint32_t total_levels = read_word_temp(img);
    assert(total_levels > 0);
    vector<uint32_t> locs;

    for (uint32_t i = 0; i < total_levels; ++i) {
        uint32_t loc = read_word_temp(img);
        if (loc == id) {
            shared_ptr<memory> new_memory;
            if (i == total_levels - 1) {
                // cout << "Creating DRAM controller at level " << i << "\n";
                // create dram controller
                dramController::dramController_cfg_t dc_cfgs;
                if (!new_dram) {
                    new_dram = shared_ptr<dram> (new dram ());
                }
                dc_cfgs.use_lock = true;
                dc_cfgs.off_chip_latency = 50;
                dc_cfgs.bytes_per_cycle = 8; 
                dc_cfgs.dram_process_time = 10;
                dc_cfgs.dc_process_time = 1;
                dc_cfgs.header_size_bytes = 4;
                shared_ptr<dramController> dc(new dramController(id, i+1, 
                                       tile->get_time(), tile->get_statistics(), 
                                       log, ran, new_dram, dc_cfgs));
                // Optionally initialize memory
                if (mem_size > 0) dc->mem_fill(mem_start, mem_size, m);
                new_memory = dc;
                if (last_local_cache) {
                    last_local_cache->set_home_memory(new_memory);
                    last_local_cache->set_home_location(loc, i+1);
                }
            } else {
                // cout << "Creating cache at level " << i << "\n";
                // create cache (new level of cache)
                cache::cache_cfg_t cache_cfgs;
                cache_cfgs.associativity = 1;
                cache_cfgs.block_size_bytes = 16;
                cache_cfgs.total_block = (i == 0) ? 64 : 256;
                cache_cfgs.process_time = 1;
                cache_cfgs.block_per_cycle = 1;
                cache_cfgs.policy = cache::CACHE_RANDOM;
                shared_ptr<cache> new_cache (new cache (id, i+1, 
                                       tile->get_time(), tile->get_statistics(),
                                       log, ran, cache_cfgs));
                new_memory = new_cache;
                if (last_local_cache) {
                    last_local_cache->set_home_memory(new_memory);
                    last_local_cache->set_home_location(loc, i+1);
                }
                last_local_cache = new_cache;
            }
            if (icash) add_to_i_memory_hierarchy(i+1, new_memory);
            else add_to_memory_hierarchy(i+1, new_memory);
        } else  {
            if (i == 0 && rm != NULL) {
                rm->set_remote_home(loc, i+1);
            }
            if (last_local_cache) {
                last_local_cache->set_home_memory(rm);
                last_local_cache->set_home_location(loc, i+1);
            }
            last_local_cache = shared_ptr<cache>();
        }
    }
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

void mcpu::exec_core() {
    //cout << "exec_core PID: " << get_id() << ", jump_active: " << jump_active << endl;
    data_complete();
    shared_ptr<instr> i = instruction_fetch_complete(pc);    
    if (running && i && !pending_request) {
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

/* -------------------------------------------------------------------------- */
/* Memory interface                                                           */
/* -------------------------------------------------------------------------- */

shared_ptr<instr> mcpu::instruction_fetch_complete(uint32_t pc) {
    if (pending_i_request && !pending_request /* no OoO */ &&
        nearest_i_memory()->ready(pending_instruction_reqid)) {
        shared_ptr<memoryRequest> ld_req = 
                        nearest_i_memory()->get_req(pending_instruction_reqid);
        uint32_t raw = *(ld_req->data());
        //if (get_id() == 0) printf("Completed instruction_fetch_complete, address: %x, instr: %x\n", (uint32_t) ld_req->addr(), raw);
        nearest_i_memory()->finish(pending_instruction_reqid);
        pending_i_request = false;
        return shared_ptr<instr> (new instr(raw));
    }
    if (!pending_i_request) {
        //if (get_id() == 0) printf("Issued instruction_fetch_complete, address: %x\n", pc);
        shared_ptr<memoryRequest> read_req(new memoryRequest(pc, byte_count));
        pending_instruction_reqid = nearest_i_memory()->request(read_req);
        pending_i_request = true;
    }
    return shared_ptr<instr>();
}

void mcpu::data_complete() {
    if (pending_request && nearest_memory()->ready(pending_reqid)) {
        shared_ptr<memoryRequest> req = 
            nearest_memory()->get_req(pending_reqid);
        if (req->rw() == MEM_READ) 
            set(gpr(pending_lw_gpr), data_complete_read(req));
        close_memory_op();
    }
}

uint32_t mcpu::data_complete_read(const shared_ptr<memoryRequest> &req) {
    uint32_t m;            
    switch (req->byte_count()) { 
        case 1: m = 0xFF; break;  
        case 2: m = 0xFFFF; break;
        case 3: m = 0xFFFFFF; break;
        case 4: m = 0xFFFFFFFF; break;
        default: throw exc_addr_align();
    }
    uint32_t raw = *(req->data()) & m;
    if (pending_lw_sign_extend) {
        uint32_t check = raw & (1 << (req->byte_count()*8 - 1));
        m = (check) ? 0xffffffffU << (req->byte_count()*8) : 0x0;
        raw = raw | m;
    }
    return raw;
}

inline void mcpu::close_memory_op() {
    nearest_memory()->finish(pending_reqid);
    pending_request = false;
}

void mcpu::data_fetch_read( const gpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err) {
    assert(!pending_request);
    shared_ptr<memoryRequest> read_req(new memoryRequest(addr, bytes));
    pending_reqid = nearest_memory()->request(read_req); 
    pending_lw_gpr = dst; // TODO: don't pass by copy!
    pending_lw_sign_extend = sign_extend;
    pending_request = true;
}

void mcpu::data_fetch_write(    const uint32_t &addr, 
                                const uint32_t &val, 
                                const uint32_t &bytes) throw(err) {
    assert(!pending_request);
    uint32_t wdata[1] = {val};
    shared_ptr<memoryRequest> write_req(new memoryRequest(addr, bytes, wdata));
    pending_reqid = nearest_memory()->request(write_req);
    pending_request = true;
}

/* -------------------------------------------------------------------------- */
/* MIPS helpers                                                               */
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

void mcpu::execute(shared_ptr<instr> ip) throw(err) {
    instr i = *ip;
    LOG(log, 5) << "[mcpu " << get_id() << "] "
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
    case IC_LB: data_fetch_read(i.get_rt(), mem_addr(), 1, true); break;
    case IC_LBU: data_fetch_read(i.get_rt(), mem_addr(), 1, false); break;
    case IC_LDC1: unimplemented_instr(i, pc);
    case IC_LDC2: unimplemented_instr(i, pc);
    case IC_LDXC1: unimplemented_instr(i, pc);
    case IC_LH: data_fetch_read(i.get_rt(), mem_addrh(), 2, true); break;
    case IC_LHU: data_fetch_read(i.get_rt(), mem_addrh(), 2, false); break;
    case IC_LL: unimplemented_instr(i, pc);
    case IC_LUI: set(i.get_rt(), i.get_imm() << 16); break;
    case IC_LUXC1: unimplemented_instr(i, pc);
    case IC_LW: data_fetch_read(i.get_rt(), mem_addrw(), 4, true); break;
    case IC_LWC1: unimplemented_instr(i, pc);
    case IC_LWC2: unimplemented_instr(i, pc);
    case IC_LWL: unimplemented_instr(i, pc);
    /*{ 
        uint32_t a = mem_addr();
        uint32_t w = load<uint32_t>(a);
        uint32_t m = 0xffffffffU << ((a & 0x3) << 3);
        set(i.get_rt(), combine(get(i.get_rt()), w, m));
        break;
    }*/
    case IC_LWR: unimplemented_instr(i, pc);
    /*{
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
    case IC_SB: data_fetch_write(mem_addr(), get(i.get_rt()), 1); break;
    case IC_SC: unimplemented_instr(i, pc);
    case IC_SDBBP: unimplemented_instr(i, pc);
    case IC_SDC1: unimplemented_instr(i, pc);
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
    case IC_SUB_D: unimplemented_instr(i, pc);
    case IC_SUB_PS: unimplemented_instr(i, pc);
    case IC_SUB_S: unimplemented_instr(i, pc);
    case IC_SUXC1: unimplemented_instr(i, pc);
    case IC_SW: data_fetch_write(mem_addrw(), get(i.get_rt()), 4); break;
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

void mcpu::syscall(uint32_t call_no) throw(err) {
    switch (call_no) {
    case SYSCALL_PRINT_INT:
        stdout_buffer << dec << (int32_t) get(gpr(4)); break;
    case SYSCALL_PRINT_STRING: {
        err_panic("MCPU: SYSCALL_PRINT_STRING not implemented.");
        /*
        uint32_t saddr = get(gpr(4));
        // construct string
        data_fetch_read(const gpr dst, saddr, 1, bool sign_extend)        

        string s(reinterpret_cast<char *>(ram->ptr(get(gpr(4)))));
        string::size_type last_pos = 0;
        string::size_type eol_pos = 0;
        while ((eol_pos = s.find('\n', last_pos)) != string::npos) {
            stdout_buffer << string(s, last_pos, eol_pos + 1);
            flush_stdout();
            last_pos = eol_pos + 1;
        }
        stdout_buffer << string(s, last_pos);
        */
        break;
    }
    case SYSCALL_EXIT_SUCCESS:
        flush_stdout(); running = false; break;
    case SYSCALL_EXIT:
        flush_stdout(); running = false; break;
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

