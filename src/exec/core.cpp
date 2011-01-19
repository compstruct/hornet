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

/* Never used */
void core::add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err) { assert(false); }
bool core::work_queued() throw(err) { assert(false); return false; }
bool core::is_ready_to_offer() throw(err) { assert(false); return false; }
void core::set_stop_darsim() throw(err) { assert(false); }
