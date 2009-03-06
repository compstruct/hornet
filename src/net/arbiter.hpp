// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ARBITER_HPP__
#define __ARBITER_HPP__

#include "node.hpp"

typedef enum {
    AS_NONE = 0,
    AS_DUMB = 1,
    NUM_AS
} arbitration_t;

class arbiter : public clockable {
public:
    arbiter(shared_ptr<node> src, shared_ptr<node> dst, arbitration_t scheme,
            shared_ptr<logger> log = shared_ptr<logger>(new logger()))
        throw(err);
    virtual ~arbiter() throw();
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
private:
    arbitration_t scheme;
    node_id src_id;
    node_id dst_id;
    shared_ptr<connection> src_to_dst;
    shared_ptr<connection> dst_to_src;
    shared_ptr<logger> log;
private:
    arbiter();                // not implemented
    arbiter(const arbiter &); // not implemented
};

#endif // __ARBITER_HPP__
