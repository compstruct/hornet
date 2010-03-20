// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <climits>
#include "cstdint.hpp"
#include "tile.hpp"

tile::tile(const tile_id &new_id, const uint32_t num_tiles,
           const uint64_t &init_time, const uint64_t &stats_t0,
           shared_ptr<flow_rename_table> flow_renames,
           logger &new_log) throw()
    : id(new_id), time(init_time), pes(), nodes(), bridges(), arbiters(),
      stats(new tile_statistics(time, stats_t0, flow_renames)),
      log(new_log) {
    uint32_t num_id_bits = 0;
    for (uint32_t t = num_tiles-1; t; ++num_id_bits, t >>= 1);
    uint32_t pid_bits = sizeof(packet_id) * CHAR_BIT;
    uint32_t id_offset = pid_bits - num_id_bits;
    packet_id mask = ~((1ULL << id_offset) - 1ULL);
    packet_id templ =
        static_cast<packet_id>(id.get_numeric_id()) << id_offset;
    pid_factory =
        shared_ptr<id_factory<packet_id> >(new id_factory<packet_id>(templ,
                                                                     mask));
}

void tile::add(shared_ptr<pe> p) throw() {
    for (pes_t::const_iterator i = pes.begin(); i != pes.end(); ++i) {
        assert((*i)->get_id() != p->get_id());
    }
    pes.push_back(p);
}

void tile::add(shared_ptr<bridge> b) throw() {
    for (bridges_t::const_iterator i = bridges.begin();
         i != bridges.end(); ++i) {
        assert((*i)->get_id() != b->get_id());
    }
    bridges.push_back(b);
}

void tile::add(shared_ptr<node> n) throw() {
    for (nodes_t::const_iterator i = nodes.begin(); i != nodes.end(); ++i) {
        assert((*i)->get_id() != n->get_id());
    }
    nodes.push_back(n);
}

void tile::add(shared_ptr<arbiter> a) throw() {
    for (arbiters_t::const_iterator i = arbiters.begin();
         i != arbiters.end(); ++i) {
        assert((*i)->get_id() != a->get_id());
    }
    arbiters.push_back(a);
}

const uint64_t &tile::get_time() const throw() {
    return time;
}

shared_ptr<tile_statistics> tile::get_statistics() const throw() {
    return stats;
}

shared_ptr<id_factory<packet_id> > tile::get_packet_id_factory() const throw() {
    return pid_factory;
}

void tile::tick_positive_edge() throw(err) {
    for (pes_t::iterator i = pes.begin(); i != pes.end(); ++i) {
        (*i)->tick_positive_edge();
    }
    for (nodes_t::iterator i = nodes.begin(); i != nodes.end(); ++i) {
        (*i)->tick_positive_edge();
    }
    for (bridges_t::iterator i = bridges.begin(); i != bridges.end(); ++i) {
        (*i)->tick_positive_edge();
    }
    for (arbiters_t::iterator i = arbiters.begin(); i != arbiters.end(); ++i) {
        (*i)->tick_positive_edge();
    }
}

void tile::tick_negative_edge() throw(err) {
    for (pes_t::iterator i = pes.begin(); i != pes.end(); ++i) {
        (*i)->tick_negative_edge();
    }
    for (nodes_t::iterator i = nodes.begin(); i != nodes.end(); ++i) {
        (*i)->tick_negative_edge();
    }
    for (bridges_t::iterator i = bridges.begin(); i != bridges.end(); ++i) {
        (*i)->tick_negative_edge();
    }
    for (arbiters_t::iterator i = arbiters.begin(); i != arbiters.end(); ++i) {
        (*i)->tick_negative_edge();
    }
    ++time;
}

void tile::fast_forward_time(uint64_t new_time) throw() {
    assert(new_time >= time);
    assert(is_drained());
    time = new_time;
}

bool tile::is_ready_to_offer() const throw() {
    for (pes_t::const_iterator i = pes.begin(); i != pes.end(); ++i) {
        if ((*i)->is_ready_to_offer()) return true;
    }
    return false;
}

bool tile::work_queued() const throw() {
    for (pes_t::const_iterator i = pes.begin(); i != pes.end(); ++i) {
        if ((*i)->work_queued()) return true;
    }
    return false;
}

uint64_t tile::next_pkt_time() const throw() {
    uint64_t next_time = UINT64_MAX;
    for (pes_t::const_iterator i = pes.begin(); i != pes.end(); ++i) {
       uint64_t t = (*i)->next_pkt_time();
       if (next_time > t) next_time = t;
    }
    return next_time;
}

bool tile::is_drained() const throw() {
    for (pes_t::const_iterator i = pes.begin(); i != pes.end(); ++i) {
        if (!(*i)->is_drained()) return false;
    }
    for (nodes_t::const_iterator i = nodes.begin(); i != nodes.end(); ++i) {
        if (!(*i)->is_drained()) return false;
    }
    for (bridges_t::const_iterator i = bridges.begin();
         i != bridges.end(); ++i) {
        if (!(*i)->is_drained()) return false;
    }
    return true;
}
