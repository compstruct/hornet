// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "logger.hpp"

logstreambuf::logstreambuf() throw() : streams(), msg_verb(0) { }
logstreambuf::~logstreambuf() throw() { }

void logstreambuf::add(streambuf *s, const verbosity &v) throw() {
    streams.push_back(pair<verbosity,streambuf *>(v,s));
}

logger::logger() throw() : ostream(&buf), buf(), owned_streams() { }
logger::~logger() throw() { }

logger &logger::add(ostream &s, const verbosity &v) throw() {
    buf.add(s.rdbuf(), v);
    return *this;
}

logger &logger::add(const shared_ptr<ostream> s, const verbosity &v) throw() {
    owned_streams.push_back(s);
    buf.add(s->rdbuf(), v);
    return *this;
}

ostream &operator<<(logger &out, const verbosity &v) {
    out.buf.set_message_verbosity(v);
    return out;
}

