// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __EVENT_PARSER_HPP__
#define __EVENT_PARSER_HPP__

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <memory>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "injector.hpp"

using namespace std;
using namespace boost;

class event_parser {
public:
    typedef vector<std::shared_ptr<injector> > injectors_t;
    typedef map<flow_id, node_id> flow_starts_t;
public:
    event_parser(std::shared_ptr<vector<string> > event_files,
                 std::shared_ptr<injectors_t> injectors,
                 std::shared_ptr<flow_starts_t> flow_starts);
private:
    typedef std::tuple<string,uint64_t> pos_t;
    uint64_t p_nat(uint64_t low = 0);
    string p_kw(const set<string> &kws, bool empty_ok);
    string p_kw(const string &kw1, bool empty_ok);
    string p_kw(const string &kw1, const string &kw2, bool empty_ok);
    void p_flow(const flow_id &flow);
    void p_line();
private:
    std::shared_ptr<injectors_t> injectors;
    std::shared_ptr<flow_starts_t> flow_starts;
    std::shared_ptr<istream> input;
    std::shared_ptr<istream> line;
    pos_t pos;
    uint64_t cur_tick;
};

#endif // __EVENT_PARSER_HPP__
