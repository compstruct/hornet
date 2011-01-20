// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "core.hpp"

core::core(const pe_id &id, const uint64_t &t,
           shared_ptr<id_factory<packet_id> > pif,
           shared_ptr<tile_statistics> st, logger &l,
           shared_ptr<random_gen> r) throw(err)
    : pe(id),
      system_time(t),
      m_packet_id_factory(pif),
      stats(st), log(l),
      ran(r) { } 

core::~core() throw() { }

void core::connect(shared_ptr<bridge> net_bridge) throw(err) {
    m_net = net_bridge;
    shared_ptr<vector<uint32_t> > qs = m_net->get_ingress_queue_ids();
    m_queue_ids.clear();
    copy(qs->begin(), qs->end(), back_insert_iterator<vector<uint32_t> >(m_queue_ids));
}

void core::release_xmit_buffer() {
    /* release xmit buffer for injection if transmission is done */
    for (map<uint32_t, uint64_t*>::iterator i = m_xmit_buffer.begin(); i != m_xmit_buffer.end(); ++i) {
        if (m_net->get_transmission_done(i->first)) {
            delete i->second;
            m_xmit_buffer.erase(i->first);
            break;
        }
    }
}

void core::tick_positive_edge() throw(err) {

    release_xmit_buffer();

    /* Send message(s) in out_msg_queues */

    /* accept all new requests in memory components (they will begin working in this cycle) */
    for (vector<shared_ptr<memory> >::iterator i = m_first_memories.begin(); i != m_first_memories.end(); ++i) {
        (*i)->initiate();
    }
    
    /* execute core / memory server. */
    /* they will check if the requests they issued are done */
    /* they may also make new requests (which will be accepted in the next cycle, and will not begin working in this cycle) */
    exec_core();
    exec_mem_server();

    /* check if requests issued by middle-level memory components are done */
    for (vector<shared_ptr<memory> >::iterator i = m_first_memories.begin(); i != m_first_memories.end(); ++i) {
        (*i)->update();
    }

    /* serve accepted memory requests (the results will be updated to parents in the next cycle) */ 
    for (vector<shared_ptr<memory> >::iterator i = m_first_memories.begin(); i != m_first_memories.end(); ++i) {
        (*i)->process();
    }

    /* receive message(s) and put into correpsonding in_msg_queues */
}

void core::tick_negative_edge() throw(err) {
}

void core::exec_mem_server() {
}

/* Never used */
void core::add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err) { assert(false); }
bool core::work_queued() throw(err) { assert(false); return false; }
bool core::is_ready_to_offer() throw(err) { assert(false); return false; }
void core::set_stop_darsim() throw(err) { assert(false); }
