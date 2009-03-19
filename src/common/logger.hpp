// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __LOGGER_HPP__
#define __LOGGER_HPP__

#include <vector>
#include <iostream>
#include <boost/shared_ptr.hpp>

using namespace std;
using namespace boost;

class verbosity {
public:
    explicit verbosity(unsigned) throw();
    ~verbosity() throw();
    bool operator<(const verbosity &) throw();
    bool operator<=(const verbosity &) throw();
    bool operator==(const verbosity &) throw();
    bool operator>=(const verbosity &) throw();
    bool operator>(const verbosity &) throw();
private:
    unsigned verb;
};

class logstreambuf : public streambuf {
public:
    logstreambuf() throw();
    virtual ~logstreambuf() throw();
    void add(streambuf *, const verbosity &v) throw();
    void set_message_verbosity(const verbosity &) throw();
protected:
    virtual int overflow(int);
private:
    vector<pair<verbosity, streambuf *> > streams;
    verbosity msg_verb; // current message verbosity
};

class logger : public ostream {
public:
    logger() throw();
    virtual ~logger() throw();
    logger &add(ostream &, const verbosity &) throw();
    logger &add(shared_ptr<ostream>, const verbosity &) throw();
    friend ostream &operator<<(logger &, const verbosity &);
private:
    logstreambuf buf;
    vector<shared_ptr<ostream> > owned_streams;
};

inline verbosity::verbosity(unsigned v) throw() : verb(v) { }

inline verbosity::~verbosity() throw() { }

inline
bool verbosity::operator<(const verbosity &v) throw() { return verb<v.verb; }

inline
bool verbosity::operator<=(const verbosity &v) throw() { return verb<=v.verb; }

inline
bool verbosity::operator==(const verbosity &v) throw() { return verb==v.verb; }

inline
bool verbosity::operator>(const verbosity &v) throw() { return verb>v.verb; }

inline
bool verbosity::operator>=(const verbosity &v) throw() { return verb>=v.verb; }

inline int logstreambuf::overflow(int ch) {
    for (vector<pair<verbosity, streambuf *> >::iterator si = streams.begin();
         si != streams.end(); ++si) {
        if (msg_verb <= si->first) si->second->sputc(ch);
    }
    return 0;
}

inline void logstreambuf::set_message_verbosity(const verbosity &v) throw() {
    msg_verb = v;
}

ostream &operator<<(logger &out, const verbosity &v);

#endif // __LOGGER_HPP__

