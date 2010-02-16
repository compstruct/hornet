// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __EVENT_PARSER_HPP__
#define __EVENT_PARSER_HPP__

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "injector.hpp"

using namespace std;
using namespace boost;

class event_parser {
public:
    typedef vector<shared_ptr<injector> > injectors_t;
    typedef map<flow_id, node_id> flow_starts_t;
public:
    event_parser(shared_ptr<vector<string> > event_files,
                 shared_ptr<injectors_t> injectors,
                 shared_ptr<flow_starts_t> flow_starts) throw(err);
private:
    typedef tuple<string,unsigned> pos_t;
    unsigned p_nat(unsigned low = 0) throw(err);
    string p_kw(const set<string> &kws, bool empty_ok) throw(err);
    string p_kw(const string &kw1, bool empty_ok) throw(err);
    string p_kw(const string &kw1, const string &kw2, bool empty_ok) throw(err);
    void p_flow(const flow_id &flow) throw(err);
    void p_line() throw(err);
private:
    shared_ptr<injectors_t> injectors;
    shared_ptr<flow_starts_t> flow_starts;
    shared_ptr<istream> input;
    shared_ptr<istream> line;
    pos_t pos;
    unsigned cur_tick;
};

#endif // __EVENT_PARSER_HPP__
