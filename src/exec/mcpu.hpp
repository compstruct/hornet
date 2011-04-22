// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MCPU_HPP__
#define __MCPU_HPP__

#include <iostream>
#include <sstream>
#include <iomanip>
#include <boost/shared_ptr.hpp>
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

using namespace std;
using namespace boost;

/* Master TODO list
   
   [Known] Bugs:
    1.) When a double is stored or loaded from memory and crosses a cache line 
        boundary, an exception is thrown.  Right now, we set huge cache lines 
        to make it very unlikely that this will happen.  We should properly 
        handle this case in the future.
        ---> see if you can get the compiler to ensure that this doesn't happen
    2.) when __H_enable_memory_hierarchy is called, data collected by 
        __H_read_line is lost.  Fix: place the __H_read_line result in a 
        temporary buffer (dest) and move it over the final buffer manually (done 
        circa 2/11/11 in blackscholes.c) 

   TODO:
    1.) Get fopen to support more than one file, and 'w' modes. 
    2.) EM2_PURE make sure that when the guest context slot is empty, that the 
        native context can execute at 100% throughput

   Common Qs (?):
    1.) why does enable_memory hierarchy exist?
    Changes to benchmarks:
    1.) In structs, etc... change datatypes that are < 32b to 32b. (word    
        alignment---see OptionType in blackscholes.OptionData_

    ASSUMPTIONS:
    1.) when tid == get_id(), the thread is at its native core.  This assumption 
        is used in the code at label 'A'.

    PERFORMANCE MODEL:
    1.) talk about instructions, data
    2.) EM2_PURE: models load/unload delay, but is non-blocking when multiple 
        loads/unloads happen in an overlapping fashion. 
        (THIS IS NOT REALISTIC!!!---should be changed)
    3.) EM2_PURE only waits for a single instruction to have been completed when 
        preventing evictions from happening.  IF A MEMORY REQUEST FOR A TO-BE-
        EVICTED CONTEXT IS OUTSTANDING, BUT THAT CONTEXT HAS EXECUTED ONE 
        INSTRUCTION, IT WILL BE EVICTED IMMEDIATELY.
    4.) Evictions do not happen when a guest core is waiting on a memory 
        instruction or a jump (the evictions wait)

    Notes:
    1.) Context size is probably going to be larger than 512... as we have to
        support FP registers.
*/
/* List of parameters

    1.) EM2_PURE: toggle whether EM2 is to be used.  (this has to be set at the 
        ./configure step)
    2.) PROFILE: various forms of execution profiling.
    3.) PIPELINED: should the MIPS core be a magic single-cycle processor, or 
        should it have a variable instruction delay model?
*/
/* System
    - MIPS 32 ISA
    - 1 Branch delay slot
*/
class mcpu : public core {
public:
    explicit mcpu(  uint32_t                            num_nodes,
                    const pe_id                         &id, 
                    const uint64_t                      &time, 
                    uint32_t                            pc, 
                    uint32_t                            stack_pointer,
                    shared_ptr<id_factory<packet_id> >  pif,
                    shared_ptr<tile_statistics>         stats,
                    logger                              &log,
                    shared_ptr<random_gen>              r,
                    core_cfg_t                          core_cfgs) 
                    throw(err);
    virtual ~mcpu() throw();

// parameters ------------------------------------------------------------------

private:
#ifdef EM2_PURE
    // TODO: move these into a separate core_cfg type
    const static int FLITS_PER_MESSAGE = 2;
    const static int CONTEXT_LOAD_LATENCY = 5;
    const static int CONTEXT_UNLOAD_LATENCY = 5; // core -> network

    const static int MIGRATION_CHANNEL = 0;
    const static int EVICTION_CHANNEL = 1;
#endif
    const static int MAX_BUFFER_SIZE = 256; // used by syscalls
    const static int32_t BYTE_COUNT = 4; // for the MIPS ISA  
    const static int NUM_REGISTERS = 32;

// flags -----------------------------------------------------------------------

    // Debugging flags
    const static bool DEBUG_EM2 = false;
    const static bool DEBUG_INSTR = false;
    const static bool DEBUG_PIPELINE = false;
    const static bool DEBUG_GPR = false;

    // Profiling flags
    const static bool PROFILE = false;    
    const static bool PROFILE_INSTRUCTIONS = false;
    const static bool PROFILE_MEMORY = true;

    // microarchitecture flags
    const static bool PIPELINED = true;

// profiling properties --------------------------------------------------------

    const static uint32_t PROFILE_CORE = 0;        

    // allows for simulation to be terminated early
    const static uint32_t PROFILE_START_TIME = 0;
    const static uint32_t PROFILE_END_TIME = 1000;

// core microarchitecture ------------------------------------------------------

    const static int PIPELINE_STAGES = 5;

// core interface --------------------------------------------------------------

public:
    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_drained() const throw();
    virtual void exec_core();

// mips scalar -----------------------------------------------------------------

private:
    uint32_t get(const gpr &r) const throw();
    uint32_t get_s(const fpr &r) const throw();
    uint64_t get_d(const fpr &r) const throw();        
    uint32_t get(const hwr &r) const throw(exc_reserved_hw_reg);
    bool get_cp1_cf(const cfr &r) const throw();
    void set(const gpr &r, uint32_t val) throw();
    void set_s(const fpr &r, uint32_t v) throw();
    void set_d(const fpr &r, uint64_t v) throw();
    void set_cp1_cf(const cfr &r, bool value) throw();
    void set_hi_lo(uint64_t val) throw();
    void execute(shared_ptr<instr> ip) throw(err);

private:
    static bool * running_array; // per core
    static ostringstream * stdout_buffer_array;
    static bool enable_memory_hierarchy;

private:
    uint64_t instr_count;
    bool interrupts_enabled;
    uint64_t hi_lo;
    shared_ptr<bridge> net;

private:
    void syscall(uint32_t syscall_no) throw(err);
    void trap() throw(err);
    void flush_stdout() throw();

// profiling -------------------------------------------------------------------

private:
    uint32_t PROFILE_COUNT_ARITHMETIC;
    uint32_t PROFILE_COUNT_MULTDIV;
    uint32_t PROFILE_COUNT_BRANCH;
    uint32_t PROFILE_COUNT_JUMP;
    uint32_t PROFILE_COUNT_MEMORY;
    uint32_t PROFILE_COUNT_FLOATING;
    uint32_t PROFILE_COUNT_MISC;

    bool early_terminated;

// Microarchitecture: Pipelining -----------------------------------------------

    int pipeline_ptr_head; // responsible for executing instructions and managing per-instruction delay
    int pipeline_ptr_tail; // responsible for fetching instructions and keeping the pipe full
    shared_ptr<instr> pipeline_instr[PIPELINE_STAGES];
    bool pipeline_valid[PIPELINE_STAGES];
    int pipeline_delay;
    bool pipeline_delay_started;
    bool purge_incoming_i_req; // kills old instruction requests in the pipe when the pipe gets flushed

    inline void reset_pipeline(uint32_t pc) {
        get_context()->tail_pc = pc;
        pipeline_ptr_head = (PIPELINE_STAGES > 1) ? 1 : 0;
        pipeline_ptr_tail = 0;
        flush_pipeline();
        pipeline_delay = 0;
        pipeline_delay_started = false;
    }
    inline void spin_pipeline_ptr_head() {
        if (pipeline_ptr_head == PIPELINE_STAGES - 1) pipeline_ptr_head = 0;
        else pipeline_ptr_head++;
    }
    inline void spin_pipeline_ptr_tail() {
        if (pipeline_ptr_tail == PIPELINE_STAGES - 1) pipeline_ptr_tail = 0;
        else pipeline_ptr_tail++;
    }
    inline void flush_pipeline() {
        if (DEBUG_PIPELINE) cout << "[mcpu " << get_id() << "] Pipeline flush." << endl;
        for (int i = 0; i < PIPELINE_STAGES; i++) pipeline_valid[i] = false;
    }
    inline bool pipeline_empty() {
        for (int i = 0; i < PIPELINE_STAGES; i++) if (pipeline_valid[i]) return false;
        return true;
    }
    inline void print_pipeline() {
        for (int i = 0; i < PIPELINE_STAGES; i++) {
            cout << "[mcpu " << get_id() << "] Pipe[" << i;
            if (i == pipeline_ptr_head) cout << ",H";
            if (i == pipeline_ptr_tail) cout << ",T";
            cout << "]: ";
            if (pipeline_valid[i]) {
                instr inst = *pipeline_instr[i];
                cout << inst;
                if (i == pipeline_ptr_head) cout << " (Delay left: " << pipeline_delay << ")";
            } else { cout << "INVALID"; }
            cout << endl;
        }
        cout << endl;
    }   

// contexts --------------------------------------------------------------------

private:
    int num_contexts; // degree of multi-threading
    int current_context;

    typedef struct {
        int tid;
        uint32_t pc;
        uint32_t gprs[NUM_REGISTERS];
        uint32_t fprs[NUM_REGISTERS];
        uint8_t fpcfs; 
    } mcpu_network_context_t;

    typedef struct {
        // state / network state
        ////////// DO NOT CHANGE THE ORDER OF THESE ELEMENTS //////////
        // (mcpu_network_context_t depends on this order for copying)
        // TODO change these elements to be inside an instance of mcpu_network_context_t
        int tid;
        uint32_t pc;
        uint32_t gprs[NUM_REGISTERS];
        uint32_t fprs[NUM_REGISTERS];
        uint8_t fpcfs; // condition flags, coprocessor 1
        ///////////////////////////////////////////////////////////////
        // state
        uint32_t tail_pc;
        bool active;
        bool executed_one_instruction; // prevents eviction deadlock
        // jumps
        bool jump_active;
        unsigned jump_time;
        uint32_t jump_target;
#ifdef EM2_PURE
        // migrations: core -> network (departures)
        bool migration_active;
        unsigned migration_time;
        mcpu_network_context_t migration_outgoing; // for overlapping load/unload
        uint32_t migration_target;
        // un migrations: network -> core (arrivals)
        bool un_migration_active;
        mcpu_network_context_t un_migration_incoming; // for overlapping load/unload
        unsigned un_migration_time;
#endif
        // instruction memory requests
        mreq_id_t pending_i_reqid;
        bool pending_i_request;
        // data memory requests
        mreq_id_t pending_reqid;
        shared_ptr<memoryRequest> pending_req;
        bool pending_request_gpr;
        bool pending_request_fpr;
        bool pending_request_memory;
        bool pending_lw_sign_extend;
        gpr pending_lw_gpr;
        fpr pending_lw_fpr;
    } mcpu_context_t;   

    mcpu_context_t * contexts;

    inline mcpu_context_t * get_context() { 
        return &contexts[current_context]; 
    }
    inline mcpu_context_t * get_context(int num) {
        assert(num >= 0 && num < num_contexts); 
        return &contexts[num]; 
    }
    inline void spin_context() {
        if (current_context == num_contexts - 1) current_context = 0;
        else current_context++;
    }
    inline bool core_occupied() {
        for (int i = 0; i < num_contexts; i++) 
            if (contexts[i].active) return true;
        return false;
    }

    inline void print_registers(int context_slot) {
        for (int i = 0; i < 32; i++) {
            printf( "[mcpu %d, %d, %d] R[%d] = %x\n", 
                    get_id().get_numeric_id(), context_slot, contexts[context_slot].tid,
                     i, get_context()->gprs[i]);
        }
    }

// EM^2 ------------------------------------------------------------------------

#ifdef EM2_PURE

private:
    shared_ptr<catStrip> cat;

    bool migrate(shared_ptr<memoryRequest> req) {
        uint32_t req_core_id = cat->getCoreID(req);
        return req_core_id != get_id().get_numeric_id();
    }

    void context_cpy(void * dst, const void * src, uint32_t context_slot) {
        //printf("[mcpu %d, %d] Moving context {tid: %d, pc: %x}\n", 
        //        get_id().get_numeric_id(), context_slot, *((uint32_t *) src), *((uint32_t *) src+1));
        memcpy(dst, src, sizeof(mcpu_network_context_t));
    }

    // Unload: core -> network
    void unload_context_start(uint32_t context_slot, uint32_t destination_core) {      
        if (DEBUG_EM2)
            printf("[mcpu %d, %d] Unload START ~~ Migrating TID %d to dest core %d (will start at PC: %x)\n", 
                    get_id().get_numeric_id(), context_slot, contexts[context_slot].tid, destination_core, contexts[context_slot].pc);
        assert(contexts[context_slot].active == true);
        //assert(contexts[context_slot].jump_active == false);
        assert(contexts[context_slot].migration_active == false);
        assert(contexts[context_slot].un_migration_active == false);

        // we don't know what memory requests will be in flight when a migration happens... we just have to be sure to kill them when they complete
        //assert(contexts[context_slot].pending_request_gpr == false);
        //assert(contexts[context_slot].pending_request_fpr == false);
        //assert(contexts[context_slot].pending_request_memory == false);

        contexts[context_slot].active = false;
        contexts[context_slot].executed_one_instruction = false;
        contexts[context_slot].migration_active = true;
        contexts[context_slot].migration_time = CONTEXT_UNLOAD_LATENCY + system_time;
        contexts[context_slot].migration_target = destination_core;

        context_cpy((void *) &contexts[context_slot].migration_outgoing, (const void *) &contexts[context_slot], context_slot);
    }
    void unload_context_finish(uint32_t context_slot) {
        if (DEBUG_EM2)
            printf( "[mcpu %d, %d] Unload END, slot %d\n", 
                    get_id().get_numeric_id(), context_slot, context_slot);
        assert(contexts[context_slot].active == false);
        //assert(contexts[context_slot].jump_active == false);
        assert(contexts[context_slot].migration_active == true);
        //assert(contexts[context_slot].un_migration_active == false); can load and unload simultaneously
        //assert(contexts[context_slot].pending_request_gpr == false);
        //assert(contexts[context_slot].pending_request_fpr == false);
        //assert(contexts[context_slot].pending_request_memory == false);

        contexts[context_slot].migration_active = false;

        shared_ptr<coreMessageQueue> send_queue(core_send_queue(MIGRATION_CHANNEL));

        void * migration_context = (void *) malloc(sizeof(mcpu_network_context_t));
        context_cpy(migration_context, (const void *) &contexts[context_slot].migration_outgoing, context_slot);

        msg_t send_msg;
        send_msg.dst = contexts[context_slot].migration_target;
        send_msg.flit_count = FLITS_PER_MESSAGE;
        send_msg.core_msg.context = migration_context;
        // send_msg.mem_msg = ...; we are migrating ~ don't set this!
        send_queue->push_back(send_msg);
    }

    // Load: network -> core
    void load_context_start(void * network_context, uint32_t context_slot) {
        if (DEBUG_EM2)
            printf( "[mcpu %d, %d] Load START, slot: %d\n", 
                    get_id().get_numeric_id(), context_slot, context_slot);
        if (contexts[context_slot].active) {
            assert(contexts[context_slot].migration_active == false);
            assert(contexts[context_slot].executed_one_instruction == true);
        } else {
            // same scenario as above (see unload_context_start)
            //assert(contexts[context_slot].jump_active == false);
            //assert(contexts[context_slot].pending_request_gpr == false);
            //assert(contexts[context_slot].pending_request_fpr == false);
            //assert(contexts[context_slot].pending_request_memory == false);
        }
        assert(contexts[context_slot].un_migration_active == false);

        context_cpy(&contexts[context_slot].un_migration_incoming, network_context, context_slot);

        contexts[context_slot].un_migration_active = true;
        contexts[context_slot].un_migration_time = CONTEXT_LOAD_LATENCY + system_time;
    }
    void load_context_finish(uint32_t context_slot) {
        assert(contexts[context_slot].active == false);
        //assert(contexts[context_slot].executed_one_instruction == false); slot has already been cleared (active == false)
        //assert(contexts[context_slot].jump_active == false);
        assert(contexts[context_slot].migration_active == false);
        assert(contexts[context_slot].un_migration_active == true);
        //assert(contexts[context_slot].pending_request_gpr == false);
        //assert(contexts[context_slot].pending_request_fpr == false);
        //assert(contexts[context_slot].pending_request_memory == false);

        context_cpy(&contexts[context_slot], &contexts[context_slot].un_migration_incoming, context_slot);

        contexts[context_slot].active = true;
        contexts[context_slot].un_migration_active = false;
        if (DEBUG_EM2)
            printf( "[mcpu %d, %d] Load END, slot %d.  Starting PC: %x\n", 
                    get_id().get_numeric_id(), context_slot, context_slot, contexts[context_slot].pc);
    }

#endif

// memory hierarchy ------------------------------------------------------------

private: // instruction memory hierarchy
    map<int, shared_ptr<memory> > m_i_memory_hierarchy;
    int m_i_max_memory_level;
    int m_i_min_memory_level;
    void add_to_i_memory_hierarchy(int level, shared_ptr<memory> mem);

private: // instruction/data interface

    /*  Terminology:
            fetch - send a MemoryRequest to the memory hierarchy
            complete -  check the MemoryRequest's status, and finish() if it has 
                        returned
            read - lw for MIPS
            write - sw for MIPS
    */

    // instruction fetch/complete interface
    shared_ptr<instr> instruction_fetch_complete(uint32_t pc);

    // data {read,write}{fetch,complete} interface
    void data_complete();
    void data_complete_helper(const shared_ptr<memoryRequest> &req);
    inline bool pending_memory_request() { 
        return pending_request(current_context);
    }
    inline bool conservative_pending_memory_request() {
        for (int i = 0; i < num_contexts; i++) {
            if (pending_request(i)) return true;        
        }
        return false;
    }
    inline bool pending_request(int context) { 
        return  contexts[context].pending_request_gpr | 
                contexts[context].pending_request_fpr | 
                contexts[context].pending_request_memory; 
    }
    inline void close_memory_op() {
        nearest_memory()->finish(get_context()->pending_reqid);
        get_context()->pending_req = shared_ptr<memoryRequest>(); // NULL        
        get_context()->pending_request_gpr = false;
        get_context()->pending_request_fpr = false;
        get_context()->pending_request_memory = false;
    }
    bool data_fetch_to_gpr( const gpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err);
    bool data_fetch_to_fpr( const fpr dst,
                            const uint32_t &addr, 
                            const uint32_t &bytes) throw(err);
    bool data_fetch_read(   const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err);
    bool data_fetch_write(  const uint32_t &addr, 
                            const uint64_t &val, 
                            const uint32_t &bytes) throw(err);

protected: // memory hierarchy processing & inherited members
    inline shared_ptr<memory> nearest_i_memory() {
        return m_i_memory_hierarchy[m_i_min_memory_level];
    }
    virtual void mh_initiate();
    virtual void mh_update();
    virtual void mh_process();

public: // called by sys.cpp
    void initialize_memory_hierarchy(   uint32_t id, shared_ptr<tile> tile,
                                        shared_ptr<ifstream> img, 
                                        bool icash,
                                        shared_ptr<remoteMemory> rm,
                                        uint32_t mem_start,
                                        uint32_t mem_size, 
                                        shared_ptr<mem> memory_backing_store,
                                        shared_ptr<dram> dram_backing_store);

// utility  --------------------------------------------------------------------

private:      
    int fid_key;
    FILE * fid_value;
    shared_ptr<dramController> backingDRAM_data;
};

/* -------------------------------------------------------------------------- */
/* MIPS                                                                       */
/* -------------------------------------------------------------------------- */

inline uint32_t mcpu::get(const gpr &r) const throw() {
    return r.get_no() == 0 ? 0 : contexts[current_context].gprs[r.get_no()];
}
inline uint32_t mcpu::get_s(const fpr &r) const throw() {
    return contexts[current_context].fprs[r.get_no()];
}
inline uint64_t mcpu::get_d(const fpr &r) const throw() {
    assert(r.get_no() % 2 == 0);
    uint64_t b = 0x00000000ffffffffULL & contexts[current_context].fprs[r.get_no()];
    uint64_t t = (((uint64_t) contexts[current_context].fprs[r.get_no()+1]) << 32);
    uint64_t ret = t | b;
    //if (get_id() == 0) 
    //    printf( "[mcpu %d] Got: %016llX from f[%d]&f[%d]\n",  get_id().get_numeric_id(), 
    //        (long long unsigned int) ret, r.get_no(), r.get_no()+1);
    return ret;
}
inline uint32_t mcpu::get(const hwr &r) const throw(exc_reserved_hw_reg) {
    switch (r.get_no()) {
    case 0: return get_id().get_numeric_id();
    case 1: return 0;
    case 2: return system_time;
    case 3: return 1; // spec ambiguous on whether cc_res is 3 or 4
    case 4: return 1; // spec ambiguous on whether cc_res is 3 or 4
    case 30: return 0x52697375;
    case 31: return 0x52697375;
    default: throw exc_reserved_hw_reg(r.get_no());
    }
}
inline bool mcpu::get_cp1_cf(const cfr &r) const throw() {
    assert(r.get_no() >= 0 && r.get_no() <= 7);
    uint8_t mask = 1 << r.get_no();
    return contexts[current_context].fpcfs & mask;
}

inline void mcpu::set(const gpr &r, uint32_t v) throw() {
    if (r.get_no() != 0) {
        if (DEBUG_GPR)
            cout << "[mcpu " << get_id() << ", " << current_context << ", " << get_context()->tid << "] " 
                 << r << " <- " << hex << setfill('0') << setw(8) << v << endl;
        contexts[current_context].gprs[r.get_no()] = v;
    }
}
inline void mcpu::set_s(const fpr &r, uint32_t v) throw() {
    if (DEBUG_GPR)
        LOG(log,5) << "[mcpu.fp " << get_id() << "]     " << r << " <- "
                   << hex << setfill('0') << setw(8) << v << endl;
    contexts[current_context].fprs[r.get_no()] = v;
}
inline void mcpu::set_d(const fpr &r, uint64_t v) throw() {
    assert(r.get_no() % 2 == 0);
    uint32_t b = (uint32_t) v;
    uint32_t t = (uint32_t) (v >> 32);
    contexts[current_context].fprs[r.get_no()] = b;
    contexts[current_context].fprs[r.get_no()+1] = t;
    //if (get_id() == 0) 
    //    printf( "[mcpu %d] Set: f[%d]=%x,f[%d]=%x\n",  get_id().get_numeric_id(), 
    //        r.get_no(), b, r.get_no()+1, t);
}
inline void mcpu::set_hi_lo(uint64_t v) throw() {
    LOG(log,5) << "[mcpu " << get_id() << "]     hi,lo <- "
        << hex << setfill('0') << setw(16) << v << endl;
    hi_lo = v;
}
inline void mcpu::set_cp1_cf(const cfr &r, bool value) throw() {
    assert(r.get_no() >= 0 && r.get_no() <= 7);
    uint8_t mask = 1 << r.get_no();
    if (value) mask = ~mask;
    contexts[current_context].fpcfs &= mask;
}

#endif // __MCPU_HPP__



