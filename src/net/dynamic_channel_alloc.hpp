// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __DYNAMIC_CHANNEL_ALLOC_HPP__
#define __DYNAMIC_CHANNEL_ALLOC_HPP__

#include <set>
#include <map>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include "channel_alloc.hpp"
#include "egress.hpp"

class dynamic_channel_alloc : public channel_alloc {
public:
    dynamic_channel_alloc(node_id src, logger &log) throw();
    virtual ~dynamic_channel_alloc() throw();
    virtual void allocate() throw(err);
    void add_egress(shared_ptr<egress> egress) throw(err);
private:
    typedef vector<shared_ptr<egress> > egresses_t;
    egresses_t egresses;
};

#endif // __DYNAMIC_CHANNEL_ALLOC_HPP__
