// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "logger.hpp"

logstreambuf::logstreambuf() : streams(), msg_verb(0) { }
logstreambuf::~logstreambuf() { }

void logstreambuf::add(streambuf *s, int v) {
    streams.push_back(tuple<int,streambuf *>(v,s));
}

logger::logger()
    : ostream(&buf), max_verbosity(0), buf(), owned_streams() { }

logger::~logger() { }

void logger::add(ostream &s, int v) {
    if (v > max_verbosity) max_verbosity = v;
    buf.add(s.rdbuf(), v);
}

void logger::add(const shared_ptr<ostream> s, int v) {
    if (v > max_verbosity) max_verbosity = v;
    owned_streams.push_back(s);
    buf.add(s->rdbuf(), v);
}

