// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceCore.hpp"

memtraceCore::memtraceCore(const pe_id &id, const uint64_t &t,
                           shared_ptr<id_factory<packet_id> > pif,
                           shared_ptr<tile_statistics> st, logger &l,
                           shared_ptr<random_gen> r,
                           shared_ptr<memtraceThreadPool> pool,
                           /* progress tracker */
                           memtraceCore_cfg_t cfgs) throw(err) 
    : core(id, t, pif, st, l, r),
      m_cfgs(cfgs),
      m_threads(pool) {
    for (unsigned int i = 0; i < m_cfgs.max_threads; ++i) {
        lane_entry_t entry;
        entry.status = LANE_EMPTY;
        m_lanes.push_back(entry);
    }
}

memtraceCore::~memtraceCore() throw() {}

void memtraceCore::release_xmit_buffer() {
    /* release xmit buffer for injection if transmission is done */
    for (map<uint32_t, uint64_t*>::iterator i = m_xmit_buffer.begin(); i != m_xmit_buffer.end(); ++i) {
        if (m_net->get_transmission_done(i->first)) {
            delete i->second;
            m_xmit_buffer.erase(i->first);
            break;
        }
    }
}

void memtraceCore::tick_positive_edge() throw(err) {

    release_xmit_buffer();

    /* pick a thread to evict and send */
        /* candidates: status==idle || status==migrate */
    /* pick a thread to migrate and send */
        /* candidates: status==migrate */

    /* send messages(s) from message queue */

    /* iterate all marked requests in core -> check target, if DONE, status=idle, release request id   */
    /* iterate all marked requests in memory server -> check target, if DONE, put reply message in message queue */
    /* iterate all marked requests in each memory level (from the top level to the bottom) -> check target, if DONE, status=DONE */

    /* iterate all requests of all memories but remote in INIT */
        /* ->BUSY, --serve_time */
    /* iterate all requests of all memories but remote in BUSY */
        /* if serve_time == 0 and HIT : ->DONE */
        /* else if serve_time == 0 and MISS : put a request into the next level (->INIT) and mark it in internal table : ->WAIT */
   
    /* iterate all requests of remote in INIT */
        /* ->BUSY, --serve_time */
    /* iterate all request of remote in BUSY */
        /* if serve_time == 0, put request message into mesage queue : ->WAIT */

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
            cur.status = LANE_BUSY;
        }

        /* work on ALU */
        if (cur.status == LANE_BUSY) {

            cur.thread->execute();

            if (cur.thread->remaining_alu_cycle() == 0) {
                /* finished execution */
                if (cur.thread->type() == memtraceThread::INST_MEMORY) {
                    // EM case (-> LANE_MIG)
                    // RA case (request to remote, bookkeeping, -> LANE_WAIT)
                    // local L1 case (request to local L1, bookkeeping, -> LANE_WAIT)
                    cur.status = LANE_IDLE; 
                } else if (cur.thread->type() == memtraceThread::INST_OTHER) {
                    cur.status = LANE_IDLE;
                } else {
                    LOG(log,2) << "[memtraceCore:" << get_id().get_numeric_id() << "] "
                               << "finished a memtraceThread " << cur.thread->get_id()
                               << " @ " << system_time << endl;
                    unload_thread(m_lane_ptr);
                }
            }
        }

    }
                
    /* process new incoming packets */
        /* if mig_message, selectively put into a lane */
        /* if mem_message(req), put a request into remote memory (->INIT) and mark it in memory server table */
        /* if mem_message(rep), update remote memory req table (->DONE) */
}

void memtraceCore::tick_negative_edge() throw(err) {}

uint64_t memtraceCore::next_pkt_time() throw(err) { 
    if (is_drained()) {
        return UINT64_MAX;
    } else {
        return system_time;
    }
}

bool memtraceCore::is_drained() const throw() { 
    /* TODO : support fast forwarding */
    return false; 
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

