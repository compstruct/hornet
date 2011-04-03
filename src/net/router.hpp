// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#include <vector>
#include <utility>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "virtual_queue_id.hpp"
#include "ingress.hpp"
#include "egress.hpp"
#include "logger.hpp"
#include "channel_alloc.hpp"

using namespace std;
using namespace boost;

class router {
public:
    /* multi-path routing */
    typedef enum {
        RT_PROBABILITY = 0,
        RT_ADAPTIVE_QUEUE,
        RT_ADAPTIVE_PACKET
    } multi_path_routing_t;
    virtual ~router() throw();
    const node_id &get_id() const throw();
    virtual void add_ingress(shared_ptr<ingress> ingress) throw(err);
    virtual void add_egress(shared_ptr<egress> egress) throw(err) = 0;
    virtual void route() throw(err) = 0;

    inline multi_path_routing_t multi_path_routing() { return m_multi_path_routing; }
    inline void set_multi_path_routing(multi_path_routing_t type) { m_multi_path_routing = type; }

    inline void set_virtual_channel_alloc(shared_ptr<channel_alloc> vca) { m_vca = vca; }

protected:
    router(node_id id, logger &log) throw();
protected:
    const node_id id;
    typedef vector<shared_ptr<ingress> > ingresses_t;
    ingresses_t ingresses;
    logger &log;

    multi_path_routing_t m_multi_path_routing;
    shared_ptr<channel_alloc> m_vca;
};

inline const node_id &router::get_id() const throw() { return id; }

#endif // __ROUTER_HPP__

