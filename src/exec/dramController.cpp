// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "dramController.hpp"
#include "error.hpp"

dram::dram() {}
dram::~dram() {
    for (map<uint32_t, map<uint64_t, uint32_t* > >::iterator i = space.begin(); i != space.end(); ++i) {
        for (map<uint64_t, uint32_t*>::iterator j = space[i->first].begin(); j != space[i->first].end(); ++j) {
            delete[] j->second;
        }
    }
}

dramController::dramController(const uint32_t id, const uint32_t level, const uint64_t &t, 
                               shared_ptr<tile_statistics> st, logger &log, shared_ptr<random_gen> ran,
                               shared_ptr<dram> dram,
                               dramController_cfg_t cfgs)
: memory(id, level, t, st, log, ran), m_dram(dram), m_cfgs(cfgs), m_to_dram_in_transit(0), m_from_dram_in_transit(0) {
    m_channel_width = m_cfgs.bytes_per_cycle * m_cfgs.off_chip_latency;
}

dramController::~dramController() {}

mreq_id_t dramController::request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t target_level) {
    LOG(log,3) << "[dramController " << m_id << " @ " << system_time 
               << " ] received a request " << endl;
    mreq_id_t new_id = take_new_mreq_id();
    in_req_entry_t new_entry;
    new_entry.status = REQ_INIT;
    new_entry.req = req;
    m_in_req_table[new_id] = new_entry;
    return new_id;
}

shared_ptr<memoryRequest> dramController::get_req(mreq_id_t id) {
    if (m_in_req_table.count(id) > 0) {
        return m_in_req_table[id].req;
    }
    return shared_ptr<memoryRequest>();
}

bool dramController::ready(mreq_id_t id) {
    return (m_in_req_table.count(id) > 0 && m_in_req_table[id].status == REQ_DONE);
}

bool dramController::finish(mreq_id_t id) {
    if (m_in_req_table.count(id) == 0) {
        return true;
    } else if (m_in_req_table[id].status != REQ_DONE) {
        return false;
    } else {
        m_in_req_table.erase(id);
        return_mreq_id(id);
    }
    return true;
}

void dramController::initiate() {
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
        if (i->second.status == REQ_INIT) {
            i->second.status = REQ_DC_PROCESS;
            i->second.remaining_process_time = m_cfgs.dc_process_time;
        }
    }
}

void dramController::update() {
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {

        in_req_entry_t &entry = i->second;

        if (entry.status == REQ_TO_DRAM || entry.status == REQ_FROM_DRAM) {
            int done = 0;
            for (vector<on_the_fly_t>::iterator j = entry.packets.begin(); j != entry.packets.end(); ++j) {
                /* if data arrived on dram, release bandwidth */
                if ((*j).time_to_arrive <= system_time) {
                    if (entry.status == REQ_TO_DRAM) {
                        m_to_dram_in_transit -= (*j).byte_count;
                    } else {
                        m_from_dram_in_transit -= (*j).byte_count;
                    }
                    ++done;
                } else {
                    break;
                }
            }
            for (; done > 0; --done) {
                entry.packets.erase(entry.packets.begin());
            }
        }
    }
}

void dramController::process() {
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {

        in_req_entry_t &entry = i->second;

        /* process time */
        if (entry.status == REQ_DC_PROCESS || entry.status == REQ_DRAM_PROCESS) {
            --(entry.remaining_process_time);
            if (entry.remaining_process_time == 0) {
                if (entry.status == REQ_DRAM_PROCESS) {
                    (m_cfgs.use_lock) ? mem_access_safe(entry.req) : mem_access(entry.req);
                }
                entry.status = (entry.status == REQ_DC_PROCESS) ? REQ_TO_DRAM : REQ_FROM_DRAM;
                entry.bytes_to_send = m_cfgs.header_size_bytes;
                if ( (entry.req->rw() == MEM_WRITE && entry.status == REQ_TO_DRAM) ||
                     (entry.req->rw() == MEM_READ && entry.status == REQ_FROM_DRAM) ) {
                    entry.bytes_to_send += entry.req->byte_count();
                }
            }
        }

        /* send/receive from dram */
        if (entry.status == REQ_TO_DRAM || entry.status == REQ_FROM_DRAM) {
            uint32_t &in_transit = (entry.status == REQ_TO_DRAM) ? m_to_dram_in_transit : m_from_dram_in_transit;
            if (entry.bytes_to_send == 0 && entry.packets.empty()) {
                entry.status = (entry.status == REQ_TO_DRAM) ? REQ_DRAM_PROCESS : REQ_DONE;
                if (entry.status == REQ_DRAM_PROCESS) {
                    entry.remaining_process_time = m_cfgs.dram_process_time;
                }
            } else if (entry.bytes_to_send > 0 && m_channel_width > in_transit) {
                /* fairness is not guaranteed */
                uint32_t send_this_time;
                if (m_channel_width - in_transit < entry.bytes_to_send) {
                    send_this_time = m_channel_width - in_transit;
                } else {
                    send_this_time = entry.bytes_to_send;
                }
                entry.bytes_to_send -= send_this_time;
                in_transit += send_this_time;
                on_the_fly_t new_fly;
                new_fly.byte_count = send_this_time;
                new_fly.time_to_arrive = system_time + m_cfgs.off_chip_latency;
                entry.packets.push_back(new_fly);
            }
        }
    }
}

/*  decides whether to keep the input tid (if the memory access is to a per-
    thread private address space) or map the request to the shared address space 
    (arbitrarily set to -1). */
int get_tid(int proposed_tid, maddr_t addr) {
    int tid;
    int aspace = addr & 0x00400000;    
    if (aspace) {
        // private -- map to bins based on thread
        tid = proposed_tid;    
    } else {
        // shared -- always map to same bin
        tid = -1;
    }
    return tid;
}

void dramController::mem_access(shared_ptr<memoryRequest> req) {
    int tid_index = get_tid(req->tid(), req->addr());    
    for (uint32_t i = 0; i < req->byte_count(); i += 4) {
        uint64_t offset = (req->addr() + i) % DRAM_BLOCK_SIZE;
        uint64_t index = (req->addr() + i) - offset;
        if (m_dram->space[tid_index].count(index) == 0) {
            uint32_t *line = new uint32_t[DRAM_BLOCK_SIZE/4];
            if (!line) {
                throw err_out_of_mem();
            }
            /* initialize to 0 for now */
            for (uint32_t j = 0; j < DRAM_BLOCK_SIZE/4; ++j) {
                *(line+j) = 0;
            }
            m_dram->space[tid_index][index] = line; 
        }
        uint32_t *src = (req->rw() == MEM_READ)? (m_dram->space[tid_index][index]) + offset/4 : req->data() + i/4;
        uint32_t *tgt = (req->rw() == MEM_READ)? req->data() + i/4: (m_dram->space[tid_index][index]) + offset/4;
        (*tgt) = (*src);

        //
        //printf("read DRAM[%d][%x] == %x\n", tid_index, (unsigned int) req->addr() + i, *src);
        //if (req->rw() == MEM_READ)
        //    printf("read DRAM[%d][%llu][%llu] == %x\n", tid_index, (long long unsigned int) index, (long long unsigned int)  offset/4, *src);
    }
}

void dramController::mem_access_safe(shared_ptr<memoryRequest> req) {
    unique_lock<recursive_mutex> lock(m_dram->dram_mutex);
    mem_access(req);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// Addendum for MIPS frontend (CF 2/5/11)

/* effects: memcpy from this dram's backing store to a specified buffer 
            (transfers are in bytes) */
void dramController::mem_read_instant(  int proposed_tid,
                                        void * destination, 
                                        maddr_t mem_start, 
                                        size_t count,
                                        bool endianc) {
    int tid_index = get_tid(proposed_tid, mem_start);    
    for (uint32_t i = 0; i < count; i+=4) {
        uint64_t offset = (mem_start + i) % DRAM_BLOCK_SIZE;
        uint64_t index = (mem_start + i) - offset;
        uint32_t src;        
        if (m_dram->space[tid_index].count(index) > 0) 
            src = *((m_dram->space[tid_index][index]) + offset/4);
        else /* makes uncached loads/stores easier to implement */
            src = 0x0; 
        if (endianc) src = endian(src);
        uint32_t * tgt = ((uint32_t *) destination) + i/4;
        *tgt = src;

        /* Temporary debugging
        printf("%d:\n", src);
        printf("%c", (char) src);
        printf("%c", (char) (src >> 8));
        printf("%c", (char) (src >> 16));
        printf("%c\n\n", (char) (src >> 24));
        */
        //printf("read DRAM[%d][%x] == %x\n", tid_index, (unsigned int) mem_start + i, src);
    }
}

/* effects: Initializes the DRAM with data stored in source.  This function 
            should only be used to initialize memory before the program starts 
            -- i.e. used by sys.cpp. */
void dramController::mem_write_instant( int proposed_tid,
                                        shared_ptr<mem> source,
                                        uint32_t mem_start,
                                        uint32_t mem_size) {
    int tid_index = get_tid(proposed_tid, mem_start);
    for (uint32_t i = 0; i < mem_size; i += 4) {
        uint64_t offset = (mem_start + i) % DRAM_BLOCK_SIZE;
        uint64_t index = (mem_start + i) - offset;
        if (m_dram->space[tid_index].count(index) == 0) {
            uint32_t *line = new uint32_t[DRAM_BLOCK_SIZE/4];
            if (!line) throw err_out_of_mem();
            for (uint32_t j = 0; j < DRAM_BLOCK_SIZE/4; ++j)  *(line+j) = 0;
            m_dram->space[tid_index][index] = line; 
        }
        uint32_t src = source->load<uint32_t>(mem_start + i);
        uint32_t *tgt = (m_dram->space[tid_index][index]) + offset/4;
        (*tgt) = src;

        // Temporary debugging
        //printf("write init DRAM[%d][%x] <- %x\n", tid_index, (unsigned int) mem_start + i, src);
        //printf("write init DRAM[%d][%llu][%llu] == %x\n", tid_index, 
        //        (long long unsigned int) index, (long long unsigned int)  offset/4, src);
        //uint32_t *second_tgt = (m_dram->space[tid_index][index]) + (offset/4 - 1);
        //printf("Target (%p): %x, previous target (%p): %x\n", tgt, *tgt, second_tgt, *second_tgt);        
    }
}

/* effects: Writes data to backing store while the program is running.  The 
            memory hierarchy must a.) be disabled or b.) support memory 
            coherence for this function to not garble system state. */
void dramController::mem_write_instant( int proposed_tid,
                                        void * source,
                                        uint32_t mem_start,
                                        uint32_t mem_size,
                                        bool endianc) {
    int tid_index = get_tid(proposed_tid, mem_start);
    for (uint32_t i = 0; i < mem_size; i += 4) {
        uint64_t offset = (mem_start + i) % DRAM_BLOCK_SIZE;
        uint64_t index = (mem_start + i) - offset;
        if (m_dram->space[tid_index].count(index) == 0) {
            uint32_t *line = new uint32_t[DRAM_BLOCK_SIZE/4];
            if (!line) throw err_out_of_mem();
            for (uint32_t j = 0; j < DRAM_BLOCK_SIZE/4; ++j)  *(line+j) = 0;
            m_dram->space[tid_index][index] = line; 
        }
        uint32_t src = *(((uint32_t *) source) + i/4);
        if (endianc) src = endian(src);
        uint32_t *tgt = (m_dram->space[tid_index][index]) + offset/4;
        (*tgt) = src;

        // Temporary debugging
        //printf("%d\n", src);
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
