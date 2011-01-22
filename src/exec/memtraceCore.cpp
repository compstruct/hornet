// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceCore.hpp"
#include "memoryRequest.hpp"

memtraceCore::memtraceCore(const pe_id &id, const uint64_t &t,
                           shared_ptr<id_factory<packet_id> > pif,
                           shared_ptr<tile_statistics> st, logger &l,
                           shared_ptr<random_gen> r,
                           shared_ptr<memtraceThreadPool> pool,
                           /* progress tracker */
                           memtraceCore_cfg_t cfgs) throw(err) 
    : core(id, t, pif, st, l, r),
      m_cfgs(cfgs),
      m_lane_ptr(0), m_num_threads(0), m_num_natives(0), m_num_guests(0), 
      m_threads(pool) { 
    for (unsigned int i = 0; i < m_cfgs.max_threads; ++i) {
        lane_entry_t entry;
        entry.status = LANE_EMPTY;
        m_lanes.push_back(entry);
    }
}

memtraceCore::~memtraceCore() throw() {}

void memtraceCore::exec_core() {

    /* pick a thread to evict and send */
        /* candidates: status==idle || status==migrate */
    /* pick a thread to migrate and send */
        /* candidates: status==migrate */

    /* fetch & execute */
    if (m_num_threads > 0) { 
        /* Cycle-wise Round Robin */
        do { 
            m_lane_ptr = (m_lane_ptr + 1) % m_cfgs.max_threads; 
        } while (m_lanes[m_lane_ptr].status == LANE_EMPTY);
        lane_entry_t &cur = m_lanes[m_lane_ptr];
       
        /* fetch */
        if (cur.status == LANE_IDLE) {
            cur.thread->fetch();
            if (cur.thread->type() == memtraceThread::INST_NONE) {
                LOG(log,2) << "[memtraceCore:" << get_id().get_numeric_id() << "] "
                           << "finished a memtraceThread " << cur.thread->get_id()
                           << " @ " << system_time << endl;
                unload_thread(m_lane_ptr);
            } else {
                cur.evictable = true;
                cur.status = LANE_BUSY;
            }
        }

        /* work on ALU */
        if (cur.status == LANE_BUSY) {
            
            cur.thread->execute();

            if (cur.thread->remaining_alu_cycle() == 0) {
                /* finished execution */
                if (cur.thread->type() == memtraceThread::INST_MEMORY) {
                    // EM case (-> LANE_MIG)
                    // RA case (request to remote, bookkeeping, -> LANE_WAIT)

                    /* Request to local L1 */
                    assert(m_nearest_memory != shared_ptr<memory>());
                    assert(cur.thread->byte_count() > 0);

                    uint64_t addr = ran->random_range(3)*16*64;
                    addr = addr - addr%4;
                    uint32_t byte_count = 4;
                    shared_ptr<memoryRequest> req;

                    if (ran->random_range(3) < 2) {
                        req = shared_ptr<memoryRequest> (new memoryRequest(addr, byte_count));
                    } else {
                        uint32_t wdata = get_id().get_numeric_id();
                        req = shared_ptr<memoryRequest> (new memoryRequest(addr, byte_count, &wdata));
                    }

                    cur.mreq_id = m_nearest_memory->request(req);
                    cur.req = req;
                    cur.mem_to_serve = m_nearest_memory;
                    cur.status = LANE_WAIT;

                } else if (cur.thread->type() == memtraceThread::INST_OTHER) {
                    cur.status = LANE_IDLE;
                }
            }
        }

    }
                
    /* update waiting requests */
    for (vector<lane_entry_t>::iterator i = m_lanes.begin(); i != m_lanes.end(); ++i) {
        if ((*i).status == LANE_WAIT) {
            if((*i).mem_to_serve->ready((*i).mreq_id)) {
                LOG(log,3) << "[core " << get_id().get_numeric_id() << " @ " << system_time << " ] finished memory operation : "; 
                if ((*i).req->rw() == MEM_READ)  {
                    LOG(log,3) << " read ";
                } else {
                    LOG(log,3) << " written ";
                }
                for (uint32_t j = 0; j < (*i).req->byte_count()/4; ++j) {
                    LOG(log,3) << hex << (*i).req->data()[j] << dec;
                }
                LOG(log,3) <<  " on addr " << hex << (*i).req->addr() << dec; 
                (*i).status = LANE_IDLE;
                (*i).mem_to_serve->finish((*i).mreq_id);
            }
        }
    }
}

uint64_t memtraceCore::next_pkt_time() throw(err) { 
    if (is_drained()) {
        return UINT64_MAX;
    } else {
        return system_time;
    }
}

bool memtraceCore::is_drained() const throw() { 
    /* TODO : support fast forwarding */
    return m_threads->empty();
}

void memtraceCore::load_thread(memtraceThread* thread) {
    assert(m_num_threads < m_cfgs.max_threads);
    for (lane_idx_t i = 0; i < m_lanes.size(); ++i) {
        if (m_lanes[i].status == LANE_EMPTY) {
            m_lanes[i].status = LANE_IDLE;
            m_lanes[i].evictable = false;
            m_lanes[i].thread = thread;
            ++m_num_threads;
            if (m_native_list.count(thread->get_id()) > 0) {
                ++m_num_natives;
            } else {
                ++m_num_guests;
            }
            LOG(log,2) << "[memtraceCore:" << get_id().get_numeric_id() << "] "
                       << "loaded a memtraceThread " << thread->get_id()
                       << " @ " << system_time << endl;
            break;
        }
    }
}

void memtraceCore::unload_thread(lane_idx_t idx) {
    assert(m_lanes[idx].status != LANE_EMPTY);
    m_lanes[idx].status = LANE_EMPTY;
    --m_num_threads;
    if (m_native_list.count(m_lanes[idx].thread->get_id()) > 0) {
        --m_num_natives;
    } else {
        --m_num_guests;
    }
    LOG(log,2) << "[memtraceCore:" << get_id().get_numeric_id() << "] "
               << "unloaded a memtraceThread " << m_lanes[idx].thread->get_id()
               << " @ " << system_time << endl;
}

void memtraceCore::spawn(memtraceThread* thread) {
    /* register as a native context */
    m_native_list.insert(thread->get_id());
    /* load to the lane */
    load_thread(thread);
}

