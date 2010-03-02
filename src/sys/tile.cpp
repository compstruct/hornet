// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include "tile.hpp"

tile::tile(const node_id &new_id, const uint64_t &init_time,
           shared_ptr<statistics> new_stats, logger &new_log) throw()
    : id(new_id), time(init_time), pes(), nodes(), bridges(), arbiters(),
      stats(new_stats), log(new_log) { }

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
