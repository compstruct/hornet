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
#include "mcpu_props.hpp"
#include "math.h"

using namespace std;

bool * mcpu::running_array = NULL;
ostringstream * mcpu::stdout_buffer_array = NULL;
bool mcpu::enable_memory_hierarchy = false;

mcpu::mcpu( uint32_t                            num_nodes,
            const pe_id                         &new_id, 
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
                instr_count(0),
                interrupts_enabled(false), 
                net(),
                num_contexts(2), // TODO should not be fixed to 2
                current_context(0),
#ifdef EM2_PURE
                cat(shared_ptr<catStrip>(new catStrip(num_nodes, 1))),
#endif          
                fid_key(0),
                fid_value(NULL),
                backingDRAM_data(shared_ptr<dramController>()) {
#ifdef EM2_PURE
    /* EM2 will be buggy right now because when a context gets switched, there 
    is no logic to empty the pipeline.  Fix: DO NOT use pipelined execution AND 
    EM2. */
    assert(false);
#endif

    int thread_id = get_id().get_numeric_id();
   
    // Initialize profiling ----------------------------------------------------
    PROFILE_COUNT_ARITHMETIC = 0;
    PROFILE_COUNT_MULTDIV = 0;
    PROFILE_COUNT_BRANCH = 0;
    PROFILE_COUNT_JUMP = 0;
    PROFILE_COUNT_MEMORY = 0;
    PROFILE_COUNT_FLOATING = 0;
    PROFILE_COUNT_MISC = 0;

    early_terminated = false;
    // -- end

    // Initialize contexts -----------------------------------------------------
    contexts = (mcpu_context_t *) malloc(sizeof(mcpu_context_t) * num_contexts);
    for (int i = 0; i < NUM_REGISTERS; i++) { // helps with diff debugging
        get_context()->gprs[i] = 0;
        get_context()->fprs[i] = 0;
    }
    // -- end

    // Initialize per-thread static variables ----------------------------------
    if (!running_array) running_array = new bool[num_nodes];
    running_array[thread_id] = true;
    
    if (!stdout_buffer_array) stdout_buffer_array = new ostringstream[num_nodes];
    // -- end

    get_context()->tid = thread_id; // TID == PID ==> thread is at its native core
    get_context()->active = true;
    get_context()->executed_one_instruction = false;

    get_context()->pc = entry_point;
    get_context()->gprs[29] = stack_ptr;
    get_context()->jump_active = false;

#ifdef EM2_PURE
    get_context()->migration_active = false;
    get_context()->un_migration_active = false;
#endif

    get_context()->pending_i_request = false;
    get_context()->pending_req = shared_ptr<memoryRequest>();
    get_context()->pending_request_gpr = false;
    get_context()->pending_request_fpr = false;
    get_context()->pending_request_memory = false;
    get_context()->pending_lw_gpr = gpr(0);
    get_context()->pending_lw_fpr = fpr(0);

    for (int i = 1; i < num_contexts; i++) {
        get_context(i)->active = false;
        get_context(i)->executed_one_instruction = false;
#ifdef EM2_PURE
        get_context(i)->migration_active = false;
        get_context(i)->un_migration_active = false;
#endif
    }
    // -- end

    // Initialize microarchitecture --------------------------------------------
    // this has to be done after context initialization because of how we set 
    // the PC
    purge_incoming_i_req = false;
    reset_pipeline(entry_point);
    // -- end

    LOG(log,3) << "mcpu " << get_id() << " created with entry point at "
             << hex << setfill('0') << setw(8) << get_context()->pc << 
            " and stack pointer at " << setw(8) << stack_ptr << endl;
}

mcpu::~mcpu() throw() { 
    free(contexts);
    delete running_array;
}

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

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

// Copied from sys.cpp
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
                                        shared_ptr<mem> memory_backing_store,
                                        shared_ptr<dram> dram_backing_store) {

    shared_ptr<cache> last_local_cache = shared_ptr<cache>();

    uint32_t total_levels = read_word_temp(img);
    assert(total_levels > 0);
    vector<uint32_t> locs;

    for (uint32_t i = 0; i < total_levels; ++i) {
        uint32_t loc = read_word_temp(img);
        if (loc == id) {
            shared_ptr<memory> new_memory;
            if (i == total_levels - 1) {
                // cout << "Creating DRAM controller at level " << i << "\n";
                dramController::dramController_cfg_t dc_cfgs;
                if (!dram_backing_store) {
                    dram_backing_store = shared_ptr<dram> (new dram ());
                }
                dc_cfgs.use_lock = true;
                dc_cfgs.off_chip_latency = 50;
                dc_cfgs.bytes_per_cycle = 8;
                dc_cfgs.dram_process_time = 10;
                dc_cfgs.dc_process_time = 1;
                dc_cfgs.header_size_bytes = 4;
                shared_ptr<dramController> dc(new dramController(id, i+1, 
                                       tile->get_time(), tile->get_statistics(), 
                                       log, ran, dram_backing_store, dc_cfgs));
                // Optionally initialize memory.  This call does not risk memory 
                // hierarchy inconsistancy because it happens at initialization 
                // time
                if (mem_size > 0) dc->mem_write_instant(get_id().get_numeric_id(), 
                                                        memory_backing_store, 
                                                        mem_start, mem_size);
                new_memory = shared_ptr<dramController> (dc);
                // we use this to perform syscall reads directly from the dram
                if (!icash) backingDRAM_data = dc;
                if (last_local_cache) {
                    last_local_cache->set_home_memory(new_memory);
                    last_local_cache->set_home_location(loc, i+1);
                }
            } else {
                // cout << "Creating cache at level " << i << "\n";
                cache::cache_cfg_t cache_cfgs;
                cache_cfgs.associativity = 1;
                cache_cfgs.block_size_bytes = 1024;
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
    bool is_running = running_array[get_id().get_numeric_id()] && core_occupied();
    if (is_running) {
        return system_time;
    } else {
        return UINT64_MAX;
    }
}

bool mcpu::is_drained() const throw() {
    // Copied from mcpu.hpp:core_occupied() (const incompatability...)
    bool core_occupied = false;
    for (int i = 0; i < num_contexts; i++) 
        if (contexts[i].active) core_occupied = true;

    bool is_drained = !running_array[get_id().get_numeric_id()];
    //printf("[mcpu %d] Drained? %d\n", get_id().get_numeric_id(), is_drained);
    return is_drained;
}

void mcpu::flush_stdout() throw() {
    int id = get_context()->tid;
    if (!stdout_buffer_array[id].str().empty()) {
        LOG(log,0) << "[mcpu " << id << " out] " 
                   << stdout_buffer_array[id].str() << flush;
        stdout_buffer_array[id].str("");
    }
}

// Profiling
#define profile_instruction(type) { if (PROFILE_CORE == get_id().get_numeric_id() && \
                                        PROFILE && PROFILE_INSTRUCTIONS && \
                                        system_time >= PROFILE_START_TIME && \
                                        system_time <= PROFILE_END_TIME) { \
                                        type++; } }
#define profile_memory(type, addr) { if (PROFILE_CORE == get_id().get_numeric_id() && \
                                        PROFILE && PROFILE_MEMORY) { \
                                        printf("MEM %c 0x%x\n", type, addr);} }

void mcpu::exec_core() {

    // -------------------------------------------------------------------------
    // Profiling

    // 9.520 HACK: This block is a hack to allow us to sample statistics within a time interval    

    if (PROFILE) {
        if (PROFILE_END_TIME + 100 <= system_time) exit(0);
        if (early_terminated) return;
        if (PROFILE_CORE == get_id().get_numeric_id() && PROFILE_END_TIME <= system_time) {
            // NOTE: This only works when EM^2 is turned off! (for now, assumes that all threads are stationary)
#ifdef EM2_PURE
            assert(false);
#endif
            //printf("[mcpu %d] DUMPING STATS (system time: %d) \n", get_id().get_numeric_id(), (int) system_time);
            printf("INST %d,%d,%d,%d,%d,%d,%d\n",   PROFILE_COUNT_ARITHMETIC, 
                                                    PROFILE_COUNT_MULTDIV, 
                                                    PROFILE_COUNT_BRANCH, 
                                                    PROFILE_COUNT_JUMP, 
                                                    PROFILE_COUNT_MEMORY, 
                                                    PROFILE_COUNT_FLOATING, 
                                                    PROFILE_COUNT_MISC);
            early_terminated = true;
        }
    }

    // -------------------------------------------------------------------------

#ifdef EM2_PURE
    
    // -------------------------------------------------------------------------
    //                               EM^2

    //printf("[%ld] Start of EM2 block\n", system_time);

    // this code simulates load/unload migration delay
    for (int i = 0; i < num_contexts; i++) {
        if (!contexts[i].active) {
            if ((contexts[i].migration_active) && 
                (contexts[i].migration_time <= system_time)) {
                // context has been completely unloaded---add it to the network
                unload_context_finish(i);
            } else if ( (contexts[i].un_migration_active) && 
                        (contexts[i].un_migration_time <= system_time)) {
                // context has been sucessfully loaded---start normal operation
                load_context_finish(i);
            } else {
                // load/unload is happening.  Wait a few cycles...
            }
        } else {
            assert(contexts[i].migration_active == false);
            assert(contexts[i].un_migration_active == false);
        }
    }

    // this was an assumption when this code was written (see code that deals 
    // with migrating guest threads)
    assert(num_contexts == 2); 
    
    shared_ptr<coreMessageQueue> rcv_e_queue(core_receive_queue(EVICTION_CHANNEL));
    shared_ptr<coreMessageQueue> rcv_m_queue(core_receive_queue(MIGRATION_CHANNEL));

    // move evictions to native context slot
    if (rcv_e_queue->size() > 0) {
        // native contexts are exclusive
        assert(rcv_e_queue->size() == 1); 
        
        msg_t msg = rcv_e_queue->front();
        assert(rcv_e_queue->pop());
        load_context_start(msg.core_msg.context, 0);
    }

    if (rcv_m_queue->size() > 0) {
        msg_t msg = rcv_m_queue->front();
        uint32_t tid = *((uint32_t *) msg.core_msg.context);
        if (tid == get_id().get_numeric_id()) {
            // context has migrated back to native core
            assert(rcv_m_queue->pop());

            load_context_start(msg.core_msg.context, 0);
        } else {
            if (contexts[1].active == false && 
                contexts[1].un_migration_active == false) {
                // guest slot is empty---just load the arrived context
                assert(num_contexts == 2);
                assert(rcv_m_queue->pop());

                load_context_start(msg.core_msg.context, 1);
            } else if ( contexts[1].active == true &&
                        contexts[1].executed_one_instruction == true &&
                        !contexts[1].jump_active &&
                        !conservative_pending_memory_request()) { // TODO: stall eviction process for pending memory requests?
                // EVICTION: evict current guest thread, load in new context
                assert(num_contexts == 2);
                assert(rcv_m_queue->pop());
                if (DEBUG_EM2)
                    printf( "[mcpu %d] Evicting context in slot 1: %d!\n", 
                            get_id().get_numeric_id(), contexts[1].tid);
                unload_context_start(1, contexts[1].tid);
                load_context_start(msg.core_msg.context, 1);
            } else {
                // we have to wait for something... don't pull off the network yet
                bool c1 =   contexts[1].active == true &&
                            contexts[1].executed_one_instruction == false;
                bool c2 =   contexts[1].active == false &&
                            contexts[1].un_migration_active == true;
                bool c3 =   contexts[1].active == true &&
                            contexts[1].jump_active == true;
                bool c4 =   contexts[1].active == true &&
                            conservative_pending_memory_request() == true;
                assert(c1 || c2 || c3 || c4);
                if (DEBUG_EM2) {
                    if (c1) printf("[mcpu %d] Incoming context, but guest hasn't executed an instruction!\n", get_id().get_numeric_id());
                    if (c2) printf("[mcpu %d] Incoming context, but guest is still loading!\n", get_id().get_numeric_id());
                    if (c3) printf("[mcpu %d] Incoming context, but guest is jumping!\n", get_id().get_numeric_id());
                    if (c4) printf("[mcpu %d] Incoming context, but guest is memory fetching!\n", get_id().get_numeric_id());
                }
            }
        }
    }

    //                               EM^2
    // -------------------------------------------------------------------------

#endif
    
    // -------------------------------------------------------------------------
    //                         Main core operation

    if (get_context()->active) {
#ifdef EM2_PURE
        assert(get_context()->migration_active == false);
        assert(get_context()->un_migration_active == false);
#endif
        data_complete();

        // ---------------------------------------------------------------------
        // Microarchitecture: pipeline, tail

        shared_ptr<instr> ip;

        if (PIPELINED) {
            if (pipeline_valid[pipeline_ptr_tail] == false) {
                ip = instruction_fetch_complete(get_context()->tail_pc);
                if (ip) {
                    pipeline_valid[pipeline_ptr_tail] = true;
                    pipeline_instr[pipeline_ptr_tail] = shared_ptr<instr>(ip);
                    spin_pipeline_ptr_tail();
                    get_context()->tail_pc += 4;
                }
            }
        } else {
            ip = instruction_fetch_complete(get_context()->pc); // normal operation   
        }

        //
        // ---------------------------------------------------------------------

        if (running_array[get_context()->tid] && !pending_memory_request()) { // all instrs are still inorder

            // -----------------------------------------------------------------
            // Microarchitecture: pipeline, head

            if (PIPELINED) {
                if (pipeline_valid[pipeline_ptr_head] == true) {
                    if (!pipeline_delay_started) {
                        pipeline_delay = get_cycle_delay(pipeline_instr[pipeline_ptr_head]);
                        if (pipeline_delay == -1) {
                            cout << "No variable delay set for: " << *pipeline_instr[pipeline_ptr_head] << endl;
                            assert(false);
                        }
                        pipeline_delay_started = true;
                    }
                    if (DEBUG_PIPELINE) if (get_id() == 0) print_pipeline();
                    if (pipeline_delay > 0) {
                        pipeline_delay--;
                        return;
                    } 
                    if (pipeline_delay == 0) {
                        pipeline_delay_started = false;
                        pipeline_valid[pipeline_ptr_head] = false;
                        ip = shared_ptr<instr>(pipeline_instr[pipeline_ptr_head]);
                        spin_pipeline_ptr_head();
                    } else { return; }
                } else if (!pipeline_empty()) {
                    spin_pipeline_ptr_head();
                    return;
                } else { return; }
            } else {
                // UN-pipelined operation
                if (ip) {} // is the instruction here?
                else return; // if not, quit early
            }

            //
            // -----------------------------------------------------------------

            instr_count++;
            execute(ip);

            if (!pending_memory_request()) {
                // if we didn't perform a memory op, we must have completed an instruction
                // TODO: ASSUMES SINGLE CYCLE INSTRUCTIONS
                get_context()->executed_one_instruction = true;
            }

            if ((get_context()->jump_active) && 
                (get_context()->jump_time <= system_time)) {
                //if (get_id() == 0)
                //      cout << "[cpu " << get_id() << "]   pc <- "
                //           << hex << setfill('0') << setw(8)
                //           << get_context()->jump_target << endl;
                get_context()->pc = get_context()->jump_target;
                get_context()->jump_active = false;
                if (PIPELINED) {
                    reset_pipeline(get_context()->pc);
                    if (get_id() == 0)
                        cout << "";
                    purge_incoming_i_req = true;
                }
            } else { 
                get_context()->pc += 4; 
            }
        }
    }

    //                         Main core operation
    // -------------------------------------------------------------------------

    spin_context();
}

/* -------------------------------------------------------------------------- */
/* Memory interface                                                           */
/* -------------------------------------------------------------------------- */

// Instructions ----------------------------------------------------------------

shared_ptr<instr> mcpu::instruction_fetch_complete(uint32_t pc) {
    if (!enable_memory_hierarchy) {
        uint32_t rdata;
        backingDRAM_data->mem_read_instant( get_context()->tid, // no need to check requests... we never issue them 
                                            &rdata,
                                            pc, 
                                            sizeof(uint32_t)/4, 
                                            false);
        purge_incoming_i_req = false;
        return shared_ptr<instr> (new instr(rdata));
    } else {
        if (get_context()->pending_i_request && 
            !pending_memory_request() && 
            nearest_i_memory()->ready(get_context()->pending_i_reqid)) {
            shared_ptr<memoryRequest> ld_req = nearest_i_memory()->get_req(get_context()->pending_i_reqid);
            nearest_i_memory()->finish(get_context()->pending_i_reqid);
            get_context()->pending_i_request = false;
            ////////////////////////////////////////////////////////////////////
            // Instruction post-processing
            //
            //
            if (purge_incoming_i_req) {
                // throw out current instruction---basically do nothing with the data
                // this assumes that fetch queue = 1 deep (otherwise we have to purge more)
                purge_incoming_i_req = false;
            } else {
                // check to make sure the instruction fetched belongs to THIS thread
                if (ld_req->tid() == get_context()->tid) {            
                    uint32_t raw = *(ld_req->data());
                    //printf("[mcpu %d,%d] Completed instruction_fetch_complete, address: %x, instr: %x\n",
                    //            get_id().get_numeric_id(), current_context, (uint32_t) ld_req->addr(), raw);
                    return shared_ptr<instr> (new instr(raw));
                } else {
                    //printf("[mcpu %d,%d] Completed instruction_fetch_complete, address: %x --- but threw instruction out!\n",
                    //       get_id().get_numeric_id(), current_context, (uint32_t) ld_req->addr());
#ifndef EM2_PURE
                    assert(false); // we *should* only see this happen with EM2
#endif
                    return shared_ptr<instr>();
                }
            }
            //
            //
            ////////////////////////////////////////////////////////////////////
        }
        if (!get_context()->pending_i_request) {
            //if (get_id() == 0)
            //    printf("[mcpu %d, %d] Issued instruction_fetch to address: %x\n", 
            //           get_id().get_numeric_id(), current_context, pc);
            shared_ptr<memoryRequest> read_req(new memoryRequest(get_context()->tid, pc, BYTE_COUNT));
            get_context()->pending_i_reqid = nearest_i_memory()->request(read_req);
            get_context()->pending_i_request = true;
        }
        return shared_ptr<instr>();
    }
}

// Data ------------------------------------------------------------------------

void mcpu::data_complete() {
    if (pending_memory_request()) {
        if (!enable_memory_hierarchy) {

            if (get_context()->pending_req->tid() != get_context()->tid) { 
                //printf( "[mcpu %d,%d] Completed memory operation --- but halted/killed operation because the thread migrated!\n", 
                //        get_id().get_numeric_id(), current_context); 
                close_memory_op();
                assert(false); // we disallow this right now (by preventing evictions when a memory request is outstanding)
                return; 
            }

            if (get_context()->pending_req->rw() == MEM_READ) {    
                // for reads, get the read data manually
                uint32_t rdata[get_context()->pending_req->byte_count()/4];
                backingDRAM_data->mem_read_instant( get_context()->pending_req->tid(), 
                                                    rdata, 
                                                    get_context()->pending_req->addr(), 
                                                    get_context()->pending_req->byte_count(), 
                                                    false);
                data_complete_helper(shared_ptr<memoryRequest> (
                                  new memoryRequest(get_context()->pending_req->tid(), 
                                                    get_context()->pending_req->addr(), 
                                                    get_context()->pending_req->byte_count(), 
                                                    rdata)));                
            } else {
                assert(get_context()->pending_req->rw() == MEM_WRITE);
                // for writes, perform the write manually
                backingDRAM_data->mem_write_instant(get_context()->pending_req->tid(), 
                                                    get_context()->pending_req->data(),
                                                    get_context()->pending_req->addr(), 
                                                    get_context()->pending_req->byte_count(),
                                                    false);
            }
            get_context()->executed_one_instruction = true;
            close_memory_op();
        } else if (nearest_memory()->ready(get_context()->pending_reqid)) {
            shared_ptr<memoryRequest> req = 
                        nearest_memory()->get_req(get_context()->pending_reqid);

            if (req->tid() != get_context()->tid ||
                get_context()->pending_req->tid() != get_context()->tid) { 
                printf( "[mcpu %d, %d] Completed memory operation --- but halted/killed operation because the thread migrated!\n", 
                        get_id().get_numeric_id(), current_context); 
                close_memory_op(); 
                assert(false); // we disallow this right now (by preventing evictions when a memory request is outstanding)
                return; 
            }

            if (req->rw() == MEM_READ) data_complete_helper(req);
            get_context()->executed_one_instruction = true;
            close_memory_op();
        }
    }
}

void mcpu::data_complete_helper(const shared_ptr<memoryRequest> &req) {
    uint64_t m;
    switch (req->byte_count()) { 
        case 1: m = 0xFFULL; break;  
        case 2: m = 0xFFFFULL; break;
        case 3: m = 0xFFFFFFULL; break;
        case 4: m = 0xFFFFFFFFULL; break;
        case 8: m = 0xFFFFFFFFFFFFFFFFULL; break;
        default: throw exc_addr_align();
    }
    uint64_t raw;
    if (req->byte_count() == 8) {
        // this order corresponds to what is written in the .o file
        uint64_t bot = *(req->data()+1); 
        uint64_t top = ((uint64_t) *(req->data())) << 32;
        raw = bot | top;
    }
    else {
        raw = *(req->data());
    }
    raw = raw & m;
    if (get_context()->pending_lw_sign_extend) {
        uint32_t check = raw & (1 << (req->byte_count()*8 - 1));
        m = (check) ? 0xffffffffffffffffULL << (req->byte_count()*8) : 0x0;
        raw = raw | m;
    }
    if (get_context()->pending_request_gpr) {
        //if (get_id() == 0) 
        //    printf("[mcpu %d] Completed load gpr[%d], address: %x, data: %016llX\n", 
        //            get_id().get_numeric_id(), get_context()->pending_lw_gpr.get_no(), (uint32_t) req->addr(), 
        //            (long long unsigned int) raw); 
        set(get_context()->pending_lw_gpr, raw);
    } else {
        assert(get_context()->pending_request_fpr);
        //if (get_id() == 0) 
        //    printf("[mcpu %d] Completed load fpr[%d], address: %x, data: %f (%016llX)\n", 
        //            get_id().get_numeric_id(), get_context()->pending_lw_fpr.get_no(), (uint32_t) req->addr(),
        //            intBitsToDouble(raw), (long long unsigned int) raw);            
        if (req->byte_count() == 4)
            set_s(get_context()->pending_lw_fpr, raw);
        else if (req->byte_count() == 8) 
            set_d(get_context()->pending_lw_fpr, raw);
        else { throw exc_addr_align(); }
    }
}

/*  Note: The below functions return whether the lw/sw completed abnormally 
    (i.e. with a migration, etc). */

#define __EM2_DIVERT__  if (migrate(get_context()->pending_req)) { \
                            unload_context_start(current_context, cat->getCoreID(get_context()->pending_req)); \
                            return true; \
                        }

// Reads
bool mcpu::data_fetch_to_gpr(   const gpr dst,
                                const uint32_t &addr, 
                                const uint32_t &bytes,
                                bool sign_extend) throw(err) {
    bool divert = data_fetch_read(addr, bytes, sign_extend);
#ifdef EM2_PURE
    if (divert) return divert;
#endif
    get_context()->pending_lw_gpr = dst;
    get_context()->pending_request_gpr = true;
    return divert;
}
bool mcpu::data_fetch_to_fpr(   const fpr dst,
                                const uint32_t &addr, 
                                const uint32_t &bytes) throw(err) {
    bool divert = data_fetch_read(addr, bytes, false);
#ifdef EM2_PURE
    if (divert) return divert;
#endif
    get_context()->pending_lw_fpr = dst;
    get_context()->pending_request_fpr = true;
    return divert;
}
bool mcpu::data_fetch_read( const uint32_t &addr, 
                            const uint32_t &bytes,
                            bool sign_extend) throw(err) {
    assert(!pending_memory_request());
    profile_memory('R', addr);
    get_context()->pending_req = shared_ptr<memoryRequest> (
                     new memoryRequest(get_context()->tid, addr, bytes));
#ifdef EM2_PURE
    __EM2_DIVERT__
#endif
    if (enable_memory_hierarchy) get_context()->pending_reqid = 
                          nearest_memory()->request(get_context()->pending_req); 
    get_context()->pending_lw_sign_extend = sign_extend;
    return false;
}

// Writes
bool mcpu::data_fetch_write(    const uint32_t &addr,
                                const uint64_t &val,
                                const uint32_t &bytes) throw(err) {
    assert(!pending_memory_request());
    profile_memory('W', addr);
    uint32_t wdata[bytes/4];
    if (bytes != 8) {
        assert(bytes <= 4);
        wdata[0] = (uint32_t) val;
    } else {
        wdata[0] = (uint32_t) (val >> 32);
        wdata[1] = (uint32_t) val;
    }
    get_context()->pending_req = shared_ptr<memoryRequest> (
              new memoryRequest(get_context()->tid, addr, bytes, wdata));
#ifdef EM2_PURE
    __EM2_DIVERT__
#endif
    if (enable_memory_hierarchy) get_context()->pending_reqid = 
                          nearest_memory()->request(get_context()->pending_req);
    get_context()->pending_request_memory = true;
    return false;
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
#define branch() { get_context()->jump_active = true; get_context()->jump_time = system_time + 1; \
                   get_context()->jump_target = get_context()->pc + 4 + (i.get_simm() << 2); }
#define jump(addr) { get_context()->jump_active = true; get_context()->jump_time = system_time + 1; \
                     get_context()->jump_target = addr; }
#define branch_link() { set(gpr(31), get_context()->pc + 8); branch(); }
#define branch_now() { get_context()->pc += (i.get_simm() << 2); }
#define branch_link_now() { set(gpr(31), get_context()->pc + 8); branch_now(); }
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
#ifdef EM2_PURE
    if (DEBUG_INSTR && !get_context()->migration_active) {
#else
    if (DEBUG_INSTR) {
#endif
        if (get_id() == 0) cout << "[mcpu " << get_id() << ", " 
             << current_context << ", " << get_context()->tid << "] "
             << hex << setfill('0') << setw(8) 
             << get_context()->pc << ": " << i /*<< " (cycle #: " << system_time << ")"*/ << endl;
     //   print_registers(current_context);
    }

    instr_code code = i.get_opcode();
    switch (code) {
    case IC_ABS_D: unimplemented_instr(i, get_context()->pc);
    case IC_ABS_PS: unimplemented_instr(i, get_context()->pc);
    case IC_ABS_S: unimplemented_instr(i, get_context()->pc);
    case IC_ADD: op3ur_of(+); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_ADDI: op3si_of(+); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_ADDIU: op3si(+); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_ADDU: op3ur(+); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_ADD_D: // FP
        op3fp_d(+);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_ADD_PS: unimplemented_instr(i, get_context()->pc);
    case IC_ADD_S: // FP
        op3fp_s(+);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_ALNV_PS: unimplemented_instr(i, get_context()->pc);
    case IC_AND: op3ur(&); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_ANDI: op3ui(&); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_B: branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BAL: branch_link(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BC1F: if (!get_cp1_cf(i.get_cc())) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BC1FL: if (!get_cp1_cf(i.get_cc())) branch_link(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BC1T: if (get_cp1_cf(i.get_cc())) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BC1TL: if (get_cp1_cf(i.get_cc())) branch_link(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BC2F: unimplemented_instr(i, get_context()->pc);
    case IC_BC2FL: unimplemented_instr(i, get_context()->pc);
    case IC_BC2T: unimplemented_instr(i, get_context()->pc);
    case IC_BC2TL: unimplemented_instr(i, get_context()->pc);
    case IC_BEQ: if (get(i.get_rs()) == get(i.get_rt())) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BEQL: if (get(i.get_rs()) == get(i.get_rt())) branch_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BGEZ: if ((int32_t) get(i.get_rs()) >= 0) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BGEZAL: if ((int32_t) get(i.get_rs()) >= 0) branch_link(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BGEZALL: if ((int32_t) get(i.get_rs()) >= 0) branch_link_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BGEZL: if ((int32_t) get(i.get_rs()) >= 0) branch_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BGTZ: if ((int32_t) get(i.get_rs()) > 0) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BGTZL: if ((int32_t) get(i.get_rs()) > 0) branch_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BLEZ: if ((int32_t) get(i.get_rs()) <= 0) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BLEZL: if ((int32_t) get(i.get_rs()) <= 0) branch_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BLTZ: if ((int32_t) get(i.get_rs()) < 0) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BLTZAL: if ((int32_t) get(i.get_rs()) < 0) branch_link(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BLTZALL: if ((int32_t) get(i.get_rs()) < 0) branch_link_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BLTZL: if ((int32_t) get(i.get_rs()) < 0) branch_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BNE: if (get(i.get_rs()) != get(i.get_rt())) branch(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BNEL: if (get(i.get_rs()) != get(i.get_rt())) branch_now(); profile_instruction(PROFILE_COUNT_BRANCH); break;
    case IC_BREAK: unimplemented_instr(i, get_context()->pc);
    case IC_CACHE: unimplemented_instr(i, get_context()->pc);
    case IC_CEIL_L_D: unimplemented_instr(i, get_context()->pc);
    case IC_CEIL_L_S: unimplemented_instr(i, get_context()->pc);
    case IC_CEIL_W_D: unimplemented_instr(i, get_context()->pc);
    case IC_CEIL_W_S: unimplemented_instr(i, get_context()->pc);
    case IC_CFC1: unimplemented_instr(i, get_context()->pc);
    case IC_CFC2: unimplemented_instr(i, get_context()->pc);
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
    case IC_COP2: unimplemented_instr(i, get_context()->pc);
    case IC_CTC1: unimplemented_instr(i, get_context()->pc);
    case IC_CTC2: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_D_L: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_D_S: {
        double src = (double) intBitsToFloat(get_s(i.get_fs()));
        set_d(i.get_fd(), doubleBitsToInt(src));
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;        
    }
    case IC_CVT_D_W: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_L_D: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_L_S: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_PS_S: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_S_D: { // FP
        float src = (float) intBitsToDouble(get_d(i.get_fs()));
        set_s(i.get_fd(), floatBitsToInt(src));
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;    
    }
    case IC_CVT_S_L: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_S_PL: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_S_PU: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_S_W: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_W_D: unimplemented_instr(i, get_context()->pc);
    case IC_CVT_W_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_EQ_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_EQ_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_EQ_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_F_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_F_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_F_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_LE_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_LE_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_LE_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_LT_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_LT_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_LT_S: // FP
        opcfp_s(0, <);
        profile_instruction(PROFILE_COUNT_FLOATING); 
        break;
    case IC_C_NGE_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGE_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGE_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGLE_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGLE_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGLE_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGL_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGL_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGL_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGT_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGT_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_NGT_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_OLE_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_OLE_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_OLE_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_OLT_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_OLT_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_OLT_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_SEQ_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_SEQ_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_SEQ_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_SF_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_SF_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_SF_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_UEQ_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_UEQ_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_UEQ_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_ULE_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_ULE_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_ULE_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_ULT_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_ULT_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_ULT_S: unimplemented_instr(i, get_context()->pc);
    case IC_C_UN_D: unimplemented_instr(i, get_context()->pc);
    case IC_C_UN_PS: unimplemented_instr(i, get_context()->pc);
    case IC_C_UN_S: unimplemented_instr(i, get_context()->pc);
    case IC_DERET: unimplemented_instr(i, get_context()->pc);
    case IC_DI: interrupts_enabled = false;
    case IC_DIV: {
        int32_t rhs = get(i.get_rt());
        if (rhs != 0) {
            int32_t lhs = get(i.get_rs());
            set_hi_lo((((uint64_t) (lhs%rhs)) << 32) | ((uint32_t) (lhs/rhs)));
        }
        profile_instruction(PROFILE_COUNT_MULTDIV); 
        break;
    }
    case IC_DIVU: {
        uint32_t rhs = get(i.get_rt());
        if (rhs != 0) {
            uint32_t lhs = get(i.get_rs());
            set_hi_lo((((uint64_t) (lhs % rhs)) << 32) | (lhs / rhs));
        }
        profile_instruction(PROFILE_COUNT_MULTDIV);
        break;
    }
    case IC_DIV_D: // FP
        op3fp_d(/);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;        
    case IC_DIV_S: // FP
        op3fp_s(/);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_EHB: break;
    case IC_EI: interrupts_enabled = true;
    case IC_ERET: unimplemented_instr(i, get_context()->pc);
    case IC_EXT: set(i.get_rt(), bits(get(i.get_rs()),i.get_msb(),i.get_lsb()));
                 break;
    case IC_FLOOR_L_D: unimplemented_instr(i, get_context()->pc);
    case IC_FLOOR_L_S: unimplemented_instr(i, get_context()->pc);
    case IC_FLOOR_W_D: unimplemented_instr(i, get_context()->pc);
    case IC_FLOOR_W_S: unimplemented_instr(i, get_context()->pc);
    case IC_INS: set(i.get_rt(), splice(get(i.get_rt()), get(i.get_rs()),
                                        i.get_msb(), i.get_lsb()));
                 break;
    case IC_J: jump(((get_context()->pc + 4) & 0xf0000000) | (i.get_j_tgt() << 2)); profile_instruction(PROFILE_COUNT_JUMP); break;
    case IC_JAL: set(gpr(31), get_context()->pc + 8);
                 jump(((get_context()->pc + 4) & 0xf0000000) | (i.get_j_tgt() << 2));
                 profile_instruction(PROFILE_COUNT_JUMP);
                 break;
    case IC_JALR:
    case IC_JALR_HB: set(i.get_rd(), get_context()->pc + 8); jump(get(i.get_rs())); profile_instruction(PROFILE_COUNT_JUMP); break;
    case IC_JR:
    case IC_JR_HB: {
        //printf( "[mcpu %d, %d, %d] JR to R[%d] = %x\n", 
        //        get_id().get_numeric_id(), current_context, contexts[current_context].tid,
        //        i.get_rs().get_no(), get(i.get_rs()));
        jump(get(i.get_rs())); 
        profile_instruction(PROFILE_COUNT_JUMP);
        break;
    }
    case IC_LB: data_fetch_to_gpr(i.get_rt(), mem_addr(), 1, true); profile_instruction(PROFILE_COUNT_MEMORY); break;
    case IC_LBU: data_fetch_to_gpr(i.get_rt(), mem_addr(), 1, false); profile_instruction(PROFILE_COUNT_MEMORY); break;
    case IC_LDC1: // FP
        data_fetch_to_fpr(i.get_ft(), mem_addrw(), 8);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_LDC2: unimplemented_instr(i, get_context()->pc);
    case IC_LDXC1: unimplemented_instr(i, get_context()->pc);
    case IC_LH: data_fetch_to_gpr(i.get_rt(), mem_addrh(), 2, true); profile_instruction(PROFILE_COUNT_MEMORY); break;
    case IC_LHU: data_fetch_to_gpr(i.get_rt(), mem_addrh(), 2, false); profile_instruction(PROFILE_COUNT_MEMORY); break;
    case IC_LL: unimplemented_instr(i, get_context()->pc);
    case IC_LUI: set(i.get_rt(), i.get_imm() << 16); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_LUXC1: unimplemented_instr(i, get_context()->pc);
    case IC_LW: data_fetch_to_gpr(i.get_rt(), mem_addrw(), 4, true); profile_instruction(PROFILE_COUNT_MEMORY); break;
    case IC_LWC1: // FP
        data_fetch_to_fpr(i.get_ft(), mem_addrw(), 4);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_LWC2: unimplemented_instr(i, get_context()->pc);
    case IC_LWL: unimplemented_instr(i, get_context()->pc);
    case IC_LWR: unimplemented_instr(i, get_context()->pc);
    case IC_LWXC1: unimplemented_instr(i, get_context()->pc);
    case IC_MADD: set_hi_lo(hi_lo + ((int64_t) ((int32_t) get(i.get_rs())) *
                                     (int64_t) ((int32_t) get(i.get_rt()))));
                  profile_instruction(PROFILE_COUNT_MULTDIV);
                  break;
    case IC_MADDU: set_hi_lo(hi_lo + ((uint64_t) get(i.get_rs()) *
                                      (uint64_t) get(i.get_rt())));
                   profile_instruction(PROFILE_COUNT_MULTDIV);
                   break;
    case IC_MADD_D: unimplemented_instr(i, get_context()->pc);
    case IC_MADD_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MADD_S: unimplemented_instr(i, get_context()->pc);
    case IC_MFC0: unimplemented_instr(i, get_context()->pc);
    case IC_MFC1: // FP
        set(i.get_rt(), get_s(i.get_fs()));
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_MFC2: unimplemented_instr(i, get_context()->pc);
    case IC_MFHC1: unimplemented_instr(i, get_context()->pc);
    case IC_MFHC2: unimplemented_instr(i, get_context()->pc);
    case IC_MFHI: set(i.get_rd(), hi_lo >> 32); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_MFLO: set(i.get_rd(), hi_lo & 0xffffffffU); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_MOVE: set(i.get_rd(), get(i.get_rs())); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_MOVF: unimplemented_instr(i, get_context()->pc);
    case IC_MOVF_D: unimplemented_instr(i, get_context()->pc);
    case IC_MOVF_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MOVF_S: unimplemented_instr(i, get_context()->pc);
    case IC_MOVN: if (get(i.get_rt()) != 0) set(i.get_rd(), get(i.get_rs()));
                  profile_instruction(PROFILE_COUNT_MISC);
                  break;
    case IC_MOVN_D: unimplemented_instr(i, get_context()->pc);
    case IC_MOVN_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MOVN_S: unimplemented_instr(i, get_context()->pc);
    case IC_MOVT: unimplemented_instr(i, get_context()->pc);
    case IC_MOVT_D: unimplemented_instr(i, get_context()->pc);
    case IC_MOVT_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MOVT_S: unimplemented_instr(i, get_context()->pc);
    case IC_MOVZ: if (get(i.get_rt()) == 0) set(i.get_rd(), get(i.get_rs()));
                  profile_instruction(PROFILE_COUNT_MISC);
                  break;
    case IC_MOVZ_D: unimplemented_instr(i, get_context()->pc);
    case IC_MOVZ_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MOVZ_S: unimplemented_instr(i, get_context()->pc);
    case IC_MOV_D: // FP
        set_d(i.get_fd(), get_d(i.get_fs()));
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;        
    case IC_MOV_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MOV_S: // FP
        set_s(i.get_fd(), get_s(i.get_fs()));
        profile_instruction(PROFILE_COUNT_FLOATING); 
        break;
    case IC_MSUB: set_hi_lo(hi_lo - ((int64_t) ((int32_t) get(i.get_rs())) *
                                     (int64_t) ((int32_t) get(i.get_rt()))));
                  profile_instruction(PROFILE_COUNT_MULTDIV); 
                  break;
    case IC_MSUBU: set_hi_lo(hi_lo - ((uint64_t) get(i.get_rs()) *
                                      (uint64_t) get(i.get_rt())));
                   profile_instruction(PROFILE_COUNT_MULTDIV);
                   break;
    case IC_MSUB_D: unimplemented_instr(i, get_context()->pc);
    case IC_MSUB_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MSUB_S: unimplemented_instr(i, get_context()->pc);
    case IC_MTC0: unimplemented_instr(i, get_context()->pc);
    case IC_MTC1: // FP
        set_s(i.get_fs(), get(i.get_rt()));
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_MTC2: unimplemented_instr(i, get_context()->pc);
    case IC_MTHC1: unimplemented_instr(i, get_context()->pc);
    case IC_MTHC2: unimplemented_instr(i, get_context()->pc);
    case IC_MTHI: set_hi_lo((hi_lo & 0xffffffffU)
                            | (((uint64_t) get(i.get_rs())) << 32));
                  profile_instruction(PROFILE_COUNT_MISC);
                  break;
    case IC_MTLO: set_hi_lo((hi_lo & 0xffffffff00000000ULL)
                            | get(i.get_rs()));
                  profile_instruction(PROFILE_COUNT_MISC);
                  break;
    case IC_MUL: set(i.get_rd(),
                     (int32_t) get(i.get_rs()) * (int32_t) get(i.get_rt()));
                 profile_instruction(PROFILE_COUNT_MULTDIV);
                 break;
    case IC_MULT: set_hi_lo((int64_t) ((int32_t) get(i.get_rs())) *
                            (int64_t) ((int32_t) get(i.get_rt())));
                  profile_instruction(PROFILE_COUNT_MULTDIV);
                  break;
    case IC_MULTU: set_hi_lo((uint64_t) get(i.get_rs()) *
                             (uint64_t) get(i.get_rt()));
                   profile_instruction(PROFILE_COUNT_MULTDIV);
                   break;
    case IC_MUL_D: // FP
        op3fp_d(*);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_MUL_PS: unimplemented_instr(i, get_context()->pc);
    case IC_MUL_S: // FP
        op3fp_s(*);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_NEG_D: unimplemented_instr(i, get_context()->pc);
    case IC_NEG_PS: unimplemented_instr(i, get_context()->pc);
    case IC_NEG_S: unimplemented_instr(i, get_context()->pc);
    case IC_NMADD_D: unimplemented_instr(i, get_context()->pc);
    case IC_NMADD_PS: unimplemented_instr(i, get_context()->pc);
    case IC_NMADD_S: unimplemented_instr(i, get_context()->pc);
    case IC_NMSUB_D: unimplemented_instr(i, get_context()->pc);
    case IC_NMSUB_PS: unimplemented_instr(i, get_context()->pc);
    case IC_NMSUB_S: unimplemented_instr(i, get_context()->pc);
    case IC_NOP: profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_NOR: set(i.get_rd(), ~(get(i.get_rs()) | get(i.get_rt()))); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_OR: op3ur(|); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_ORI: op3ui(|); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_PAUSE: unimplemented_instr(i, get_context()->pc);
    case IC_PLL_PS: unimplemented_instr(i, get_context()->pc);
    case IC_PLU_PS: unimplemented_instr(i, get_context()->pc);
    case IC_PREF: unimplemented_instr(i, get_context()->pc);
    case IC_PREFX: unimplemented_instr(i, get_context()->pc);
    case IC_PUL_PS: unimplemented_instr(i, get_context()->pc);
    case IC_PUU_PS: unimplemented_instr(i, get_context()->pc);
    case IC_RDHWR: set(i.get_rt(), get(i.get_hwrd())); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_RDPGPR: unimplemented_instr(i, get_context()->pc);
    case IC_RECIP_D: unimplemented_instr(i, get_context()->pc);
    case IC_RECIP_S: unimplemented_instr(i, get_context()->pc);
    case IC_ROTR: unimplemented_instr(i, get_context()->pc); // XXX
    case IC_ROTRV: unimplemented_instr(i, get_context()->pc); // XXX
    case IC_ROUND_L_D: unimplemented_instr(i, get_context()->pc);
    case IC_ROUND_L_S: unimplemented_instr(i, get_context()->pc);
    case IC_ROUND_W_D: unimplemented_instr(i, get_context()->pc);
    case IC_ROUND_W_S: unimplemented_instr(i, get_context()->pc);
    case IC_RSQRT_D: unimplemented_instr(i, get_context()->pc);
    case IC_RSQRT_S: unimplemented_instr(i, get_context()->pc);
    case IC_SB: data_fetch_write(mem_addr(), get(i.get_rt()), 1); profile_instruction(PROFILE_COUNT_MEMORY); break;
    case IC_SC: unimplemented_instr(i, get_context()->pc);
    case IC_SDBBP: unimplemented_instr(i, get_context()->pc);
    case IC_SDC1: // FP
        data_fetch_write(mem_addrw(), get_d(i.get_ft()), 8);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_SDC2: unimplemented_instr(i, get_context()->pc);
    case IC_SDXC1: unimplemented_instr(i, get_context()->pc);
    case IC_SEB: set(i.get_rd(), (int32_t) ((int8_t) get(i.get_rt()))); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_SEH: set(i.get_rd(), (int32_t) ((int16_t) get(i.get_rt()))); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_SH: data_fetch_write(mem_addrh(), get(i.get_rt()), 2); profile_instruction(PROFILE_COUNT_MISC); break;
    case IC_SLL: set(i.get_rd(), get(i.get_rt()) << i.get_sa()); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SLLV: set(i.get_rd(), get(i.get_rt()) << get(i.get_rs())); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SLT: op3sr(<); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SLTI: op3si(<); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SLTIU: op3ui(<); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SLTU: op3ur(<); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SQRT_D: unimplemented_instr(i, get_context()->pc);
    case IC_SQRT_S: unimplemented_instr(i, get_context()->pc);
    case IC_SRA: set(i.get_rd(), ((int32_t) get(i.get_rt())) >> i.get_sa());
                 profile_instruction(PROFILE_COUNT_ARITHMETIC); 
                 break;
    case IC_SRAV: set(i.get_rd(), ((int32_t) get(i.get_rt())) >> get(i.get_rs()));
                  profile_instruction(PROFILE_COUNT_ARITHMETIC);
                  break;
    case IC_SRL: set(i.get_rd(), get(i.get_rt()) >> i.get_sa()); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SRLV: set(i.get_rd(), get(i.get_rt()) >> get(i.get_rs())); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SSNOP: break;
    case IC_SUB: op3ur_of(-); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SUBU: op3ur(-); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_SUB_D: // FP
        op3fp_d(-);
        profile_instruction(PROFILE_COUNT_FLOATING); 
        break;
    case IC_SUB_PS: unimplemented_instr(i, get_context()->pc);
    case IC_SUB_S: // FP
        op3fp_s(-);
        profile_instruction(PROFILE_COUNT_FLOATING);
        break;
    case IC_SUXC1: unimplemented_instr(i, get_context()->pc);
    case IC_SW: data_fetch_write(mem_addrw(), get(i.get_rt()), 4); profile_instruction(PROFILE_COUNT_MEMORY); break;
    case IC_SWC1: 
        data_fetch_write(mem_addrw(), get_d(i.get_ft()), 4); 
        profile_instruction(PROFILE_COUNT_MEMORY);
        break;
    case IC_SWC2: unimplemented_instr(i, get_context()->pc);
    case IC_SWL: unimplemented_instr(i, get_context()->pc);
    case IC_SWR: unimplemented_instr(i, get_context()->pc);
    case IC_SWXC1: unimplemented_instr(i, get_context()->pc);
    case IC_SYNC: unimplemented_instr(i, get_context()->pc);
    case IC_SYNCI: unimplemented_instr(i, get_context()->pc);
    case IC_SYSCALL: syscall(get(gpr(2))); break;
    case IC_TEQ: 
        if (get(i.get_rs()) == get(i.get_rt())) trap();
        break;
    case IC_TEQI: unimplemented_instr(i, get_context()->pc);
    case IC_TGE: unimplemented_instr(i, get_context()->pc);
    case IC_TGEI: unimplemented_instr(i, get_context()->pc);
    case IC_TGEIU: unimplemented_instr(i, get_context()->pc);
    case IC_TGEU: unimplemented_instr(i, get_context()->pc);
    case IC_TLBP: unimplemented_instr(i, get_context()->pc);
    case IC_TLBR: unimplemented_instr(i, get_context()->pc);
    case IC_TLBWI: unimplemented_instr(i, get_context()->pc);
    case IC_TLBWR: unimplemented_instr(i, get_context()->pc);
    case IC_TLT: unimplemented_instr(i, get_context()->pc);
    case IC_TLTI: unimplemented_instr(i, get_context()->pc);
    case IC_TLTIU: unimplemented_instr(i, get_context()->pc);
    case IC_TLTU: unimplemented_instr(i, get_context()->pc);
    case IC_TNE: unimplemented_instr(i, get_context()->pc);
    case IC_TNEI: unimplemented_instr(i, get_context()->pc);
    case IC_TRUNC_L_D: unimplemented_instr(i, get_context()->pc);
    case IC_TRUNC_L_S: unimplemented_instr(i, get_context()->pc);
    case IC_TRUNC_W_D: unimplemented_instr(i, get_context()->pc);
    case IC_TRUNC_W_S: // FP
        set_s(i.get_fd(), ((int32_t) get_s(i.get_fs())));
        profile_instruction(PROFILE_COUNT_FLOATING); 
        break;
    case IC_WAIT: unimplemented_instr(i, get_context()->pc);
    case IC_WRPGPR: unimplemented_instr(i, get_context()->pc);
    case IC_WSBH: unimplemented_instr(i, get_context()->pc);
    case IC_XOR: op3ur(^); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    case IC_XORI: op3ui(^); profile_instruction(PROFILE_COUNT_ARITHMETIC); break;
    default: unimplemented_instr(i, get_context()->pc);
    }

    // we put this comment here so that we don't have to view instructions that 
    // caused migrations twice
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
        maddr_t fname_start = (maddr_t) get(gpr(4));
        char * fname_buffer = (char *) malloc(sizeof(char) * MAX_BUFFER_SIZE);
        backingDRAM_data->mem_read_instant( get_context()->tid, 
                                            fname_buffer, fname_start,
                                            MAX_BUFFER_SIZE, true);
        // 2.) open the file & return a key
        fid_value = fopen(fname_buffer, "r");
        fid_key = (fid_value == NULL) ? 0 : 1; // TODO: turn into map for multi-file support
        free(fname_buffer);
        set(gpr(2), fid_key);
        break;
    }
    case SYSCALL_READ_LINE: {
        assert(!enable_memory_hierarchy);
        // 1.) inputs
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
            //printf("Just read: %1x\n", (unsigned)(unsigned char) lc);
            *walk = lc; walk++;
        }
        backingDRAM_data->mem_write_instant(get_context()->tid, 
                                            data_buffer, dest, 
                                            write_count, false);
        free(data_buffer);
        set(gpr(2), write_count);
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
    // DARSIM-C ----------------------------------------------------------------
    case SYSCALL_ENABLE_MEMORY_HIERARCHY: {
        // TODO: wait until we get to thread 0 again?
        //assert(false); // change this to account for migrations?
        assert(!enable_memory_hierarchy);
        enable_memory_hierarchy = true;        
        break;    
    }
    // Unached LW/SW -----------------------------------------------------------
    case SYSCALL_UNCACHED_LOAD_WORD: {
        maddr_t addr = (maddr_t) get(gpr(4));
        int loaded;
        backingDRAM_data->mem_read_instant( get_context()->tid, 
                                            &loaded, addr, 4, true);
        set(gpr(2), loaded);
        break;
    }
    case SYSCALL_UNCACHED_SET_BIT: {
        maddr_t addr = (maddr_t) get(gpr(4));
        uint32_t position = get(gpr(5));
        uint32_t mask = 0x1 << position;
        int loaded;
        backingDRAM_data->mem_read_instant( get_context()->tid, 
                                            &loaded, addr, 4, true);
        loaded = loaded | mask;
        backingDRAM_data->mem_write_instant(get_context()->tid, 
                                            &loaded, addr, 4, true);
        break;    
    }
    // Printers ----------------------------------------------------------------
    case SYSCALL_PRINT_CHAR: {
        char p = (char) get(gpr(4));
        stdout_buffer_array[get_context()->tid] << dec << p; 
        break;
    }
    case SYSCALL_PRINT_INT: {
        int p = (int32_t) get(gpr(4));
        stdout_buffer_array[get_context()->tid] << dec << p;
        break;
    }
    case SYSCALL_PRINT_FLOAT: {
        float p = intBitsToFloat(get(gpr(4)));
        stdout_buffer_array[get_context()->tid] << dec << p; 
        break;
    }
    case SYSCALL_PRINT_DOUBLE: {
        __prefix_double__
        stdout_buffer_array[get_context()->tid] << dec << in; 
        break;
    }
    case SYSCALL_PRINT_STRING: {
        maddr_t fname_start = (maddr_t) get(gpr(4));
        char * fname_buffer = (char *) malloc(sizeof(char) * MAX_BUFFER_SIZE);
        backingDRAM_data->mem_read_instant( get_context()->tid, 
                                            fname_buffer, fname_start, 
                                            MAX_BUFFER_SIZE, true);
        stdout_buffer_array[get_context()->tid] << fname_buffer;
        free(fname_buffer);
        break;
    }
    case SYSCALL_FLUSH: {
        flush_stdout();
        break;
    }
    // Exits -------------------------------------------------------------------
    case SYSCALL_EXIT_SUCCESS: {
        printf( "Context %d exit success!\n", get_context()->tid);
        flush_stdout(); 
        running_array[get_context()->tid] = false; 
        break;
    } case SYSCALL_EXIT: {
        printf( "Context %d exit!\n", get_context()->tid);
        int code = get(gpr(4));
        flush_stdout();
        running_array[get_context()->tid] = false;
        exit(code);
    }
    case SYSCALL_ASSERT: {
        int r = get(gpr(4));
        if (!r) {
            flush_stdout();
            running_array[get_context()->tid] = false;
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
    // Misc --------------------------------------------------------------------
    case SYSCALL_THREAD_ID: {
        uint32_t thread_id = get_context()->tid;
        set(gpr(2), thread_id);
        break;    
    }
  }
}

void mcpu::trap() throw(err) {
    err_panic("Trap raised!");
}


