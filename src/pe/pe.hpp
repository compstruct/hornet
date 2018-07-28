// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PE_HPP__
#define __PE_HPP__

#include <memory>
#include "cstdint.hpp"
#include "error.hpp"
#include "bridge.hpp"

using namespace std;
using namespace boost;

class pe_id {
public:
    pe_id(uint32_t new_id);
    bool operator==(const pe_id &) const;
    bool operator!=(const pe_id &) const;
    bool operator<(const pe_id &) const;
    uint32_t get_numeric_id() const;
    friend ostream &operator<<(ostream &, const pe_id &);
private:
    explicit pe_id();
    const uint32_t id;
};

inline pe_id::pe_id(uint32_t new_id) : id(new_id) { }

inline bool pe_id::operator==(const pe_id &o) const {
    return id == o.id;
}

inline bool pe_id::operator!=(const pe_id &o) const {
    return id != o.id;
}

inline bool pe_id::operator<(const pe_id &o) const {
    return id < o.id;
}

inline uint32_t pe_id::get_numeric_id() const { return id; }

ostream &operator<<(ostream &, const pe_id &);

class pe {
public:
    virtual ~pe();
    virtual void connect(std::shared_ptr<bridge> net_bridge) = 0;
    virtual void add_packet(uint64_t time, const flow_id &f, unsigned l) = 0;
    virtual bool work_queued() = 0;
    virtual void tick_positive_edge() = 0;
    virtual void tick_negative_edge() = 0;
    virtual void set_stop_darsim() = 0;
    virtual uint64_t next_pkt_time() = 0;
    virtual bool is_ready_to_offer() = 0;
    virtual bool is_drained() const = 0;
    pe_id get_id() const;
protected:
    pe(const pe_id &id);
private:
    pe_id id;
};

inline pe_id pe::get_id() const { return id; }

#endif // __PE_HPP__
