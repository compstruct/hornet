// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PE_HPP__
#define __PE_HPP__

#include <boost/shared_ptr.hpp>
#include "cstdint.hpp"
#include "error.hpp"
#include "bridge.hpp"

using namespace std;
using namespace boost;

class pe_id {
public:
    pe_id(uint32_t new_id) throw();
    bool operator==(const pe_id &) const throw();
    bool operator<(const pe_id &) const throw();
    uint32_t get_numeric_id() const throw();
    friend ostream &operator<<(ostream &, const pe_id &);
private:
    explicit pe_id() throw();
    const uint32_t id;
};

inline pe_id::pe_id(uint32_t new_id) throw() : id(new_id) { }

inline bool pe_id::operator==(const pe_id &o) const throw() {
    return id == o.id;
}

inline bool pe_id::operator<(const pe_id &o) const throw() {
    return id < o.id;
}

inline uint32_t pe_id::get_numeric_id() const throw() { return id; }

ostream &operator<<(ostream &, const pe_id &);

class pe {
public:
    virtual ~pe() throw();
    virtual void connect(shared_ptr<bridge> net_bridge) throw(err) = 0;
    virtual void add_packet(uint64_t time, const flow_id &f, unsigned l) throw(err) = 0;
    virtual bool work_queued() throw(err) = 0;
    virtual void tick_positive_edge() throw(err) = 0;
    virtual void tick_negative_edge() throw(err) = 0;
    virtual void set_stop_darsim() throw(err) = 0;
    virtual bool is_ready_to_offer() throw(err) = 0;
    virtual bool is_drained() const throw() = 0;
    pe_id get_id() const throw();
protected:
    pe(const pe_id &id) throw();
private:
    pe_id id;
};

inline pe_id pe::get_id() const throw() { return id; }

#endif // __PE_HPP__
