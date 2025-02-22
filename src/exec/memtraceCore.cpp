// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceCore.hpp"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(true) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

memtraceCore::memtraceCore(const pe_id &id, const uint64_t &t,
                           std::shared_ptr<id_factory<packet_id> > pif,
                           std::shared_ptr<tile_statistics> st, logger &l,
                           std::shared_ptr<random_gen> r,
                           std::shared_ptr<memtraceThreadPool> pool,
                           std::shared_ptr<memory> mem,
                           bool support_em,
                           uint32_t msg_queue_size,
                           uint32_t bytes_per_flit,
                           uint32_t flits_per_context,
                           uint32_t max_threads) : 
    common_core(id, t, pif, st, l, r, mem, (support_em?2:0), msg_queue_size, bytes_per_flit),
    m_support_em(support_em), m_flits_per_context(flits_per_context), m_max_threads(max_threads),
    m_lane_ptr(0), m_num_threads(0), m_num_natives(0), m_num_guests(0), 
    m_threads(pool), m_do_evict(false) 
{ 
    assert(m_flits_per_context > 0);
    assert(m_max_threads > 0);

    for (unsigned int i = 0; i < m_max_threads; ++i) {
        lane_entry_t entry;
        entry.status = LANE_EMPTY;
        m_lanes.push_back(entry);
    }
    m_pending_mig.valid = false;
}

memtraceCore::~memtraceCore() {}

void memtraceCore::execute() {

    /* update em */
    if (m_support_em) {
        if (m_pending_mig.valid) {
            if (send_queue(m_first_core_msg_type)->size() == 0 && send_queue(m_first_core_msg_type+1)->size() == 0) {
                mh_log(3) << "[thread " << m_lanes[m_pending_mig.idx].thread->get_id() << " @ " << system_time
                    << " ] has sent from " << get_id().get_numeric_id() << " to " << m_pending_mig.dst << endl;
                unload_thread(m_pending_mig.idx);
            }
            /* cancel if not sent */
            send_queue(m_first_core_msg_type)->pop();
            send_queue(m_first_core_msg_type+1)->pop();
            if (m_pending_mig.evict) {
                m_lanes[m_pending_mig.idx].thread->stats()->did_begin_eviction();
            }
            m_pending_mig.valid = false;
        }

        if (m_do_evict) {
            /* pick a thread to evict and send */
            /* only send one thread at a time */
            lane_idx_t start = ran->random_range(m_lanes.size());
            for (uint32_t i = 0; i < m_lanes.size(); ++i) {
                lane_idx_t cand = (start + i)%m_lanes.size();
                if ((m_lanes[cand].status == LANE_IDLE || m_lanes[cand].status == LANE_MIG)
#ifdef LIVELOCK_PERFORMANCE_STUDY
                    && m_lanes[cand].evictable_time < system_time && m_lanes[cand].evictable) {
#else
                    && m_lanes[cand].evictable) { 
#endif
                    std::shared_ptr<message_t> new_msg(new message_t);
                    new_msg->src = get_id().get_numeric_id();
                    new_msg->dst = m_lanes[cand].thread->native_core();
                    new_msg->flit_count = m_flits_per_context;
                    new_msg->content = m_lanes[cand].thread;
                    /* The queue is guaranteed to have a space because we always pop the message every cycle */ 
                    /* even if the message is not sent */
                    send_queue(m_first_core_msg_type)->push_back(new_msg);
                    m_pending_mig.valid = true;
                    m_pending_mig.idx = cand;
                    m_pending_mig.dst = new_msg->dst; 
                    m_pending_mig.evict = true;
                    mh_log(3) << "[thread " << m_lanes[cand].thread->get_id() << " @ " << system_time  
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
                    std::shared_ptr<message_t> new_msg(new message_t);
                    new_msg->src = get_id().get_numeric_id();
                    new_msg->dst = m_lanes[cand].req->home();
                    new_msg->flit_count = m_flits_per_context;
                    new_msg->content = m_lanes[cand].thread;
                    
                    m_pending_mig.valid = true;
                    m_pending_mig.idx = cand;
                    m_pending_mig.dst = new_msg->dst; 
                    m_pending_mig.evict = false;
                    /* The queue is guaranteed to have a space because we always pop the message every cycle */ 
                    /* even if the message is not sent */
                    if (new_msg->dst == (uint32_t)(m_lanes[cand].thread->native_core())) {
                        send_queue(m_first_core_msg_type)->push_back(new_msg);
                    } else {
                        send_queue(m_first_core_msg_type+1)->push_back(new_msg);
                    }
                    mh_log(3) << "[thread " << m_lanes[cand].thread->get_id() << " @ " << system_time
                        << " ] is a migrate candidate on " << get_id().get_numeric_id() << endl;
                    break;
                }
            }
        }
    }

    /* fetch & execute */
    if (m_num_threads > 0) { 
        /* Cycle-wise Round Robin */
        do { 
            m_lane_ptr = (m_lane_ptr + 1) % m_max_threads; 
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
                if (cur.thread->stats_enabled()) {
                    cur.thread->stats()->did_complete();
                }
                unload_thread(m_lane_ptr);
                cur.status = LANE_EMPTY;
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
                    if (cur.thread->is_read()) {
                        cur.req = std::shared_ptr<memoryRequest>(new memoryRequest(cur.thread->maddr(), 
                                                                              cur.thread->word_count(),
                                                                              cur.thread->per_mem_instr_runtime_info()));
                    } else {
                        /* this core model doesn't care data */
                        shared_array<uint32_t> dummy = shared_array<uint32_t>(new uint32_t[cur.thread->word_count()]);
                        cur.req = std::shared_ptr<memoryRequest>(new memoryRequest(cur.thread->maddr(), 
                                                                              cur.thread->word_count(), 
                                                                              dummy,
                                                                              cur.thread->per_mem_instr_runtime_info()));
                    }

                    m_memory->request(cur.req);
                    cur.status = LANE_WAIT;
                    mh_log(4) << "[thread " << cur.thread->get_id() << " @ " << system_time 
                              << " ] is making a memory request on core " 
                              << get_id().get_numeric_id() << " for address " << cur.thread->maddr() << endl;
                } else if (cur.thread->type() == memtraceThread::INST_OTHER) {
                    cur.status = LANE_IDLE;
                }
            }
        }
    }

    if (m_support_em) {
        /* load incoming threads */
        uint32_t num_arrived_natives = receive_queue(m_first_core_msg_type)->size();
        uint32_t num_arrived_guests = receive_queue(m_first_core_msg_type+1)->size();

        /* emulate one queue per each lane - can take all native contexts */
        for (uint32_t i = 0; i < num_arrived_natives; ++i) {
            std::shared_ptr<message_t> msg = receive_queue(m_first_core_msg_type)->front();
            std::shared_ptr<memtraceThread> new_thread = static_pointer_cast<memtraceThread>(msg->content);
            load_thread(new_thread);
            new_thread->stats()->did_arrive_destination();
            receive_queue(m_first_core_msg_type)->pop();
        } 
        /* may take up to num_lanes - num_native_lanes */
        uint32_t max_guests = m_max_threads - m_native_list.size();
        for (uint32_t i = 0; i < num_arrived_guests && m_num_guests < max_guests; ++i) {
            std::shared_ptr<message_t> msg = receive_queue(m_first_core_msg_type+1)->front();
            std::shared_ptr<memtraceThread> new_thread = static_pointer_cast<memtraceThread>(msg->content);
            load_thread(new_thread);
            new_thread->stats()->did_arrive_destination();
            receive_queue(m_first_core_msg_type+1)->pop();
        }
        m_do_evict = (receive_queue(m_first_core_msg_type+1)->size() > 0)? true: false;
    }
}

void memtraceCore::update_from_memory_requests() {
    /* update waiting requests */
    for (vector<lane_entry_t>::iterator i = m_lanes.begin(); i != m_lanes.end(); ++i) {
        lane_entry_t &entry = *i;
        if (entry.status == LANE_WAIT) {
            if(entry.req->status() == REQ_DONE) {
                if (entry.thread->stats_enabled()) {
                    if (entry.req->is_read()) {
                        entry.thread->stats()->did_finish_read(system_time - entry.thread->first_memory_issued_time());
                    } else {
                        entry.thread->stats()->did_finish_write(system_time - entry.thread->first_memory_issued_time());
                    }
                }
                mh_log(4) << "[core " << get_id().get_numeric_id() << " @ " << system_time 
                          << " ] finished memory operation : "; 
                if (entry.req->is_read())  {
                    mh_log(4) << " read ";
                } else {
                    mh_log(4) << " written ";
                }
                for (uint32_t j = 0; j < entry.req->word_count(); ++j) {
                    mh_log(4) << hex << entry.req->data()[j] << dec;
                }
                mh_log(4) <<  " on addr " << hex << entry.req->maddr().address << dec << endl; 
                entry.status = LANE_IDLE;
            } else if (i->req->status() == REQ_RETRY) {
                /* the memory couldn't accept the last request */
                mh_log(4) << "[thread " << entry.thread->get_id() << " @ " << system_time 
                          << " ] is making a memory RE-request on core " 
                          << get_id().get_numeric_id() << " for address " << entry.thread->maddr() << endl;

                m_memory->request(i->req); /* it's supposed to be in the positive tick of the next cycle, but doing here is equivalent */             
            } else if (m_support_em && entry.req->status() == REQ_MIGRATE) {
                mh_log(4) << "[thread " << entry.thread->get_id() << " @ " << system_time 
                          << " ] need to migrate from " << get_id().get_numeric_id() << " to core " << entry.req->home()
                          << " for address " << entry.thread->maddr() << endl;
                entry.thread->reset_current_instruction();
                entry.status = LANE_MIG;
                entry.thread->stats()->did_begin_migration();
            }
        }
    }
}

uint64_t memtraceCore::next_pkt_time() { 
    if (is_drained()) {
        m_memory->turn_off();
        return UINT64_MAX;
    } else {
        return system_time;
    }
}

bool memtraceCore::is_drained() const { 
    return m_threads->empty();
}

void memtraceCore::load_thread(std::shared_ptr<memtraceThread> thread) {
    assert(m_num_threads < m_max_threads);
    for (lane_idx_t i = 0; i < m_lanes.size(); ++i) {
        if (m_lanes[i].status == LANE_EMPTY) {
            m_lanes[i].status = LANE_IDLE;
            m_lanes[i].evictable = false;

#ifdef LIVELOCK_PERFORMANCE_STUDY
            /* livelock performance study */
            m_lanes[i].evictable_time = system_time + APPROVED_VISIT_PERIOD;
#endif

            m_lanes[i].thread = thread;
            ++m_num_threads;
            if (m_native_list.count(thread->get_id()) > 0) {
                ++m_num_natives;
            } else {
                ++m_num_guests;
            }
            mh_log(2) << "[thread " << thread->get_id() << " @ " << system_time
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
    mh_log(2) << "[thread " << m_lanes[idx].thread->get_id() << " @ " << system_time 
        << " ] is unloaded on " << get_id().get_numeric_id() << endl;
}

void memtraceCore::spawn(std::shared_ptr<memtraceThread> thread) {
    /* register as a native context */
    m_native_list.insert(thread->get_id());
    thread->set_native_core(get_id().get_numeric_id());
    if (thread->stats_enabled()) {
        thread->stats()->did_spawn();
    }
    /* load to the lane */
    load_thread(thread);
}

