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
      m_threads(pool) {}

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
        /* round robin to the first one with status==idle || status==busy */
        /* if status==idle, fetch and --alu_time, status=busy */
        /* if status==busy && alu_time == 0 && memory && !em && !ra */
            /* put a request to l1 memory (->INIT) and mark it in the lane */
        /* if status==busy && alu_time == 0 && memory && !em && ra */
            /* put a request to remote memory (->INIT) and mark it in the lane */
        /* else if status==busy && alu_time == 0 && memory && em, status=migrate */
        /* else if status==busy && alu_time == 0 && !memory, status=idle */
        /* else if status==busy && alu_time > 0, --alu_time */

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

