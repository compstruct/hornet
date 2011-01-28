// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceCore.hpp"
#include "memoryRequest.hpp"

memtraceCore::memtraceCore(const pe_id &id, const uint64_t &t,
                           shared_ptr<id_factory<packet_id> > pif,
                           shared_ptr<tile_statistics> st, logger &l,
                           shared_ptr<random_gen> r,
                           shared_ptr<memtraceThreadPool> pool,
                           memtraceCore_cfg_t cfgs, core_cfg_t core_cfgs) throw(err) 
    : core(id, t, pif, st, l, r, core_cfgs),
      m_cfgs(cfgs),
      m_lane_ptr(0), m_num_threads(0), m_num_natives(0), m_num_guests(0), 
      m_threads(pool), m_do_evict(false) { 
    for (unsigned int i = 0; i < m_cfgs.max_threads; ++i) {
        lane_entry_t entry;
        entry.status = LANE_EMPTY;
        m_lanes.push_back(entry);
    }
    m_pending_mig.valid = false;
}

memtraceCore::~memtraceCore() throw() {}

#define HIGH_PRIORITY_CHANNEL 0
#define LOW_PRIORITY_CHANNEL 1

void memtraceCore::exec_core() {

    /* update em */
    if (m_pending_mig.valid) {
        if (core_send_queue(HIGH_PRIORITY_CHANNEL)->size() == 0 && core_send_queue(LOW_PRIORITY_CHANNEL)->size() == 0) {
            LOG(log,3) << "[thread " << m_lanes[m_pending_mig.idx].thread->get_id() << " @ " << system_time
                       << " ] has sent from " << get_id().get_numeric_id() << " to " << m_pending_mig.dst << endl;
            unload_thread(m_pending_mig.idx);
        }
        /* cancel if not sent */
        core_send_queue(HIGH_PRIORITY_CHANNEL)->pop();
        core_send_queue(LOW_PRIORITY_CHANNEL)->pop();
        m_pending_mig.valid = false;
    }

    if (m_do_evict) {
        /* pick a thread to evict and send */
        /* only send one thread at a time */
        lane_idx_t start = ran->random_range(m_lanes.size());
        for (uint32_t i = 0; i < m_lanes.size(); ++i) {
            lane_idx_t cand = (start + i)%m_lanes.size();
            if ((m_lanes[cand].status == LANE_IDLE || m_lanes[cand].status == LANE_MIG)
                    && m_lanes[cand].evictable) { 
                msg_t new_msg;
                new_msg.dst = m_lanes[cand].thread->native_core();
                new_msg.flit_count = m_cfgs.flits_per_mig;
                new_msg.core_msg.context = (void*)(m_lanes[cand].thread);
                core_send_queue(HIGH_PRIORITY_CHANNEL)->push_back(new_msg);
                m_pending_mig.valid = true;
                m_pending_mig.idx = cand;
                m_pending_mig.dst = new_msg.dst; 
                LOG(log,3) << "[thread " << m_lanes[cand].thread->get_id() << " @ " << system_time  
                           << " ] is an evict candidate on " << get_id().get_numeric_id() << endl;
                break;
            }
        }
    } else {
        /* pick a thread to migrate and send */
        lane_idx_t start = ran->random_range(m_lanes.size());
        for (uint32_t i = 0; i < m_lanes.size(); ++i) {
            lane_idx_t cand = (start + i)%m_lanes.size();
            if ( m_lanes[cand].status == LANE_MIG ) {
                msg_t new_msg;
                new_msg.dst = m_lanes[cand].thread->home();
                new_msg.flit_count = m_cfgs.flits_per_mig;
                new_msg.core_msg.context = (void*)(m_lanes[cand].thread);
                m_pending_mig.valid = true;
                m_pending_mig.idx = cand;
                m_pending_mig.dst = new_msg.dst; //TODO erase
                if (new_msg.dst == (uint32_t)(m_lanes[cand].thread->native_core())) {
                    core_send_queue(HIGH_PRIORITY_CHANNEL)->push_back(new_msg);
                } else {
                    core_send_queue(LOW_PRIORITY_CHANNEL)->push_back(new_msg);
                }
                LOG(log,3) << "[thread " << m_lanes[cand].thread->get_id() << " @ " << system_time
                           << " ] is a migrate candidate on " << get_id().get_numeric_id() << endl;
                break;
            }
        }
    }

    /* fetch & execute */
    if (m_num_threads > 0) { 
        /* Cycle-wise Round Robin */
        do { 
            m_lane_ptr = (m_lane_ptr + 1) % m_cfgs.max_threads; 
        } while (m_lanes[m_lane_ptr].status == LANE_EMPTY);
        lane_entry_t &cur = m_lanes[m_lane_ptr];
       
        /* fetch */
        if (cur.status == LANE_IDLE && (m_pending_mig.valid == false || m_pending_mig.idx != m_lane_ptr)) {
            if (cur.thread->current_instruction_done()) {
                cur.thread->fetch();
            }
            if (cur.thread->type() == memtraceThread::INST_NONE) {
                LOG(log,2) << "[memtraceCore:" << get_id().get_numeric_id() << "] "
                           << "finished a memtraceThread " << cur.thread->get_id()
                           << " @ " << system_time << endl;
                stats->finish_execution(system_time);
                unload_thread(m_lane_ptr);
            } else {
                if (m_native_list.count(cur.thread->get_id()) == 0) {
                    cur.evictable = true;
                }
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
                    assert(nearest_memory() != shared_ptr<memory>());
                    assert(cur.thread->byte_count() > 0);

                    uint64_t addr = cur.thread->addr();
                    uint32_t home = cur.thread->home();
                    uint32_t byte_count = cur.thread->byte_count();
                    bool read = (cur.thread->rw() == MEM_READ);

                    if (home != get_id().get_numeric_id() || 
                        (m_cfgs.em_type == EM_NONE && m_cfgs.ra_type == RA_NONE)) {
                        /* core miss */
                        bool do_ra = false;
                        if (m_cfgs.em_type == EM_NONE) {
                            do_ra = true;
                        } else if (m_cfgs.ra_type == RA_ONLY) {
                            do_ra = true;
                        } else if (m_cfgs.ra_type == RA_RANDOM) {
                            /* 50% */
                            do_ra = (ran->random_range(2) == 0)? true : false;
                        }
                        if (do_ra) {
                            shared_ptr<memoryRequest> req;
                            if (read) {
                                req = shared_ptr<memoryRequest> (new memoryRequest(addr, byte_count));
                            } else {
                                uint32_t wdata[byte_count];
                                wdata[0] = get_id().get_numeric_id();
                                req = shared_ptr<memoryRequest> (new memoryRequest(addr, byte_count, wdata));
                            }
                            req->set_ra();

                            if (m_cfgs.library_type == LIBRARY_ONLY) {

                                /* LIBRARY_COMPETITION */
                                /* put additional information in req here, for the RA server to use */
                                /* (see exec/memoryRequest.hpp) */
                                /* example */
                                /* req->set_first_info(888); */

                                cur.mreq_id = away_cache()->request(req, home, 1);
                                cur.mem_to_serve = away_cache();

                            } else if (m_cfgs.library_type == LIBRARY_NONE) {
                                cur.mreq_id = remote_memory()->request(req, home, 1);
                                cur.mem_to_serve = remote_memory();
                            }
                            cur.req = req;
                            cur.status = LANE_WAIT;
                            if (stats->is_started()) {
                                stats->issue_memory();
                            }
                            LOG(log,1) << "[thread " << cur.thread->get_id() << " @ " << system_time 
                                       << " ] is making a remote access request on core " 
                                       << get_id().get_numeric_id() << endl;
                        } else {
                            cur.thread->reset_current_instruction();
                            cur.status = LANE_MIG;
                        }
                    } else {
                        /* core hit */
                        shared_ptr<memoryRequest> req;
                        if (read) {
                            req = shared_ptr<memoryRequest> (new memoryRequest(addr, byte_count));
                        } else {
                            uint32_t wdata = get_id().get_numeric_id();
                            req = shared_ptr<memoryRequest> (new memoryRequest(addr, byte_count, &wdata));
                        }
                        LOG(log,3) << "[thread " << cur.thread->get_id() << " @ " << system_time 
                                   << " ] is making a memory request to the nearest memory on core " 
                                   << get_id().get_numeric_id() << endl;
                        cur.mreq_id = nearest_memory()->request(req);
                        cur.req = req;
                        cur.mem_to_serve = nearest_memory();
                        cur.status = LANE_WAIT;
                        if (stats->is_started()) {
                            stats->issue_memory();
                        }
                    }
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
                if (stats->is_started()){
                    if ((*i).req->rw() == MEM_READ) {
                        stats->receive_mem_read(system_time - (*i).thread->memory_issued_time());
                    } else {
                        stats->receive_mem_write(system_time - (*i).thread->memory_issued_time());
                    }
                }

                LOG(log,1) << "[core " << get_id().get_numeric_id() << " @ " << system_time 
                           << " ] finished memory operation : "; 
                if ((*i).req->rw() == MEM_READ)  {
                    LOG(log,1) << " read ";
                } else {
                    LOG(log,1) << " written ";
                }
                for (uint32_t j = 0; j < (*i).req->byte_count()/4; ++j) {
                    LOG(log,1) << hex << (*i).req->data()[j] << dec;
                }
                LOG(log,1) <<  " on addr " << hex << (*i).req->addr() << dec << endl; 
#if 0
                cerr << "[core " << get_id().get_numeric_id() << " @ " << system_time << " ] finished memory operation : "; 
                if ((*i).req->rw() == MEM_READ)  {
                    cerr << " read ";
                } else {
                    cerr << " written ";
                }
                for (uint32_t j = 0; j < (*i).req->byte_count()/4; ++j) {
                    cerr << hex << (*i).req->data()[j] << dec;
                }
                cerr <<  " on addr " << hex << (*i).req->addr() << dec << endl; 
#endif
                (*i).status = LANE_IDLE;
                /* memtraceThread doesn't care for values, so release it */
                (*i).mem_to_serve->finish((*i).mreq_id);
            }
        }
    }

    /* load incoming threads */
    uint32_t num_arrived_natives = core_receive_queue(HIGH_PRIORITY_CHANNEL)->size();
    uint32_t num_arrived_guests = core_receive_queue(LOW_PRIORITY_CHANNEL)->size();

    /* emulate one queue per each lane - can take all native contexts */
    for (uint32_t i = 0; i < num_arrived_natives; ++i) {
        memtraceThread *new_thread;
        new_thread = (memtraceThread*)core_receive_queue(HIGH_PRIORITY_CHANNEL)->front().core_msg.context;
        load_thread(new_thread);
        core_receive_queue(HIGH_PRIORITY_CHANNEL)->pop();
    } 
    /* may take up to num_lanes - num_native_lanes */
    uint32_t max_guests = m_cfgs.max_threads - m_native_list.size();
    for (uint32_t i = 0; i < num_arrived_guests && m_num_guests < max_guests; ++i) {
        memtraceThread *new_thread;
        new_thread = (memtraceThread*)core_receive_queue(LOW_PRIORITY_CHANNEL)->front().core_msg.context;
        load_thread(new_thread);
        core_receive_queue(LOW_PRIORITY_CHANNEL)->pop();
    }
    m_do_evict = (core_receive_queue(LOW_PRIORITY_CHANNEL)->size() > 0)? true: false;
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
            LOG(log,2) << "[thread " << thread->get_id() << " @ " << system_time
                       << " ] is loaded on " << get_id().get_numeric_id() << endl;
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
    LOG(log,2) << "[thread " << m_lanes[idx].thread->get_id() << " @ " << system_time 
               << " ] is unloaded on " << get_id().get_numeric_id() << endl;
}

void memtraceCore::spawn(memtraceThread* thread) {
    /* register as a native context */
    m_native_list.insert(thread->get_id());
    thread->set_native_core(get_id().get_numeric_id());
    /* load to the lane */
    load_thread(thread);
}

