// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <iomanip>
#include <cstdlib>
#include <sstream>
#include <set>
#include <iterator>

#include "event_parser.hpp"
 
event_parser::event_parser(shared_ptr<vector<string> > events_files,
                           shared_ptr<injectors_t> injs,
                           shared_ptr<flow_starts_t> fss) throw(err)
    : injectors(injs), flow_starts(fss), input(), line(),
        pos("", 0), cur_tick(0) {
    if (!events_files) return;
    for (vector<string>::const_iterator fi = events_files->begin();
         fi != events_files->end(); ++fi) {
        input = shared_ptr<istream>(new ifstream(fi->c_str()));
        if (input->fail()) throw err_parse(*fi,"cannot open file");
        for (pos = pos_t(*fi, 1); input->good(); pos.get<1>()++) {
            string l;
            getline(*input, l);
            line = shared_ptr<istream>(new istringstream(l));
            p_line();
        }
    }
}

unsigned event_parser::p_nat(unsigned low) throw(err) {
    unsigned n;
    string s;
    *line >> s;
    if (s.size() == 0 || s[0] == '#') {
        ostringstream msg;
        msg << "found "
            << (s.size() == 0 ? "end of line" : "a comment")
            << " while expecting a number";
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
    if (s[0] == '#') {
        ostringstream msg;
        msg << "found a comment while expecting a number";
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
    char *end;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        n = strtoul(s.c_str() + 2, &end, 16);
    } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        n = strtoul(s.c_str() + 2, &end, 2);
    } else {
        n = strtoul(s.c_str(), &end, 10);
    }
    if (*end != '\0') {
        ostringstream msg;
        msg << "invalid number: \"" << s << "\"";
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
    if (n < low) {
        ostringstream msg;
        msg << "number must be at least " << low << ": " << s;
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
    return n;
}

string event_parser::p_kw(const set<string> &kws, bool empty_ok) throw(err) {
    assert(kws.size() > 0);
    string w;
    *line >> w;
    if (empty_ok && (w == "" || w[0] == '#')) return "";
    if (w == "" || w[0] == '#') {
        ostringstream msg;
        msg << "found " << (w.size() == 0 ? "end of line" : "a comment")
            << " while expecting " << (kws.size() == 1 ? "one of " : "");
        for (set<string>::iterator i = kws.begin(); i != kws.end(); ++i) {
            if (i != kws.begin()) msg << " or ";
            msg << "\"" << *i << "\"";
        }
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    } else if (kws.find(w) == kws.end()) {
        ostringstream msg;
        msg << "found \"" << w << "\" while expecting "
            << (kws.size() == 1 ? "one of " : "");
        for (set<string>::iterator i = kws.begin(); i != kws.end(); ++i) {
            if (i != kws.begin()) msg << " or ";
            msg << "\"" << *i << "\"";
        }
        msg << "\"";
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
    return w;
}

string event_parser::p_kw(const string &kw1, bool empty_ok) throw(err) {
    set<string> kws; kws.insert(kw1);
    return p_kw(kws, empty_ok);
}

string event_parser::p_kw(const string &kw1, const string &kw2,
                          bool empty_ok) throw(err) {
    set<string> kws; kws.insert(kw1); kws.insert(kw2);
    return p_kw(kws, empty_ok);
}

void event_parser::p_flow(const flow_id &flow) throw(err) {
    if (flow_starts->find(flow) == flow_starts->end()) {
        ostringstream msg;
        msg << "flow " << flow << " is not configured";
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
    node_id n = (*flow_starts)[flow];
    if (injectors->find(n) == injectors->end()) {
        ostringstream msg;
        msg << "node " << n << ", where flow " << flow << " originates, "
            << " is not an injector";
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
    uint32_t packet_size;
    uint64_t period;
    string w = p_kw("off", "size", false);
    if (w == "size") {
        packet_size = p_nat(1);
        p_kw("period", false);
        period = p_nat(1);
    } else {
        packet_size = 0;
        period = 0;
    }
    (*injectors)[(*flow_starts)[flow]]->add_event(cur_tick, flow, packet_size,
                                                  period);
}

void event_parser::p_line() throw(err) {
    *line >> skipws;
    string w = p_kw("tick", "flow", true);
    if (w.size() == 0) return;
    if (w == "tick") {
        unsigned t = p_nat();
        if (t < cur_tick) {
            ostringstream msg;
            msg << "tick " << dec << t << " precedes previous tick ("
                << cur_tick << ")";
            throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
        }
        cur_tick = t;
    } else if (w == "flow") {
        p_flow(p_nat());
    } else {
        ostringstream msg;
        msg << "bad command: \"" << w << "\"";
        throw err_parse(pos.get<0>(), pos.get<1>(), msg.str());
    }
}
