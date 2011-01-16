// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CORE_HPP__
#define __CORE_HPP__

#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include "id_factory.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "pe.hpp"
#include "random.hpp"
#include "bridge.hpp"

using namespace std;
using namespace boost;

class core : public pe {
public:
    core(const pe_id &id, const uint64_t &system_time,
         shared_ptr<id_factory<packet_id> > packet_id_factory,
         shared_ptr<tile_statistics> stats, logger &log,
         shared_ptr<random_gen> ran) throw(err);
    virtual ~core() throw();

    /* Common core methods */
    virtual void connect(shared_ptr<bridge> net_bridge) throw(err);

    /* Virtual functions */
    virtual void tick_positive_edge() throw(err) = 0;
    virtual void tick_negative_edge() throw(err) = 0;
    virtual void set_stop_darsim() throw(err) = 0;
    virtual uint64_t next_pkt_time() throw(err) = 0;
    virtual bool is_drained() const throw() = 0;

    /* TODO (Phase 5) : Design configurator methods */

protected:
    typedef struct {
        packet_id id;
        flow_id flow;
        uint32_t len;
        uint32_t xmit;
        uint64_t *payload;
    } core_incoming_packet_t;
    typedef map<uint32_t, core_incoming_packet_t> incoming_packets_t;

    /* Global time */
    const uint64_t &system_time;

    /* Network */
    shared_ptr<bridge> m_net;
    vector<uint32_t> m_queue_ids;
    incoming_packets_t m_incoming_packets;
    shared_ptr<id_factory<packet_id> > m_packet_id_factory;
    map<uint32_t, uint64_t*> m_xmit_buffer;

    /* Aux */
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;
};

/* TODO (Phase 4) : Design core stats */

#endif
