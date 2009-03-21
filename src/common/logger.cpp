// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "logger.hpp"

logstreambuf::logstreambuf() throw() : streams(), msg_verb(0) { }
logstreambuf::~logstreambuf() throw() { }

void logstreambuf::add(streambuf *s, unsigned v) throw() {
    streams.push_back(pair<unsigned,streambuf *>(v,s));
}

logger::logger() throw()
    : ostream(&buf), max_verbosity(0), buf(), owned_streams() { }

logger::~logger() throw() { }

void logger::add(ostream &s, unsigned v) throw() {
    if (v > max_verbosity) max_verbosity = v;
    buf.add(s.rdbuf(), v);
}

void logger::add(const shared_ptr<ostream> s, unsigned v) throw() {
    if (v > max_verbosity) max_verbosity = v;
    owned_streams.push_back(s);
    buf.add(s->rdbuf(), v);
}

