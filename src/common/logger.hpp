// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __LOGGER_HPP__
#define __LOGGER_HPP__

#include <vector>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;
using namespace boost;

class logstreambuf : public streambuf {
public:
    logstreambuf() throw();
    virtual ~logstreambuf() throw();
    void add(streambuf *, int verbosity) throw();
    void set_message_verbosity(int verbosity) throw();
protected:
    virtual int overflow(int);
private:
    vector<tuple<int, streambuf *> > streams;
    int msg_verb; // current message verbosity
};

class logger : public ostream {
public:
    logger() throw();
    virtual ~logger() throw();
    void add(ostream &, int) throw();
    void add(shared_ptr<ostream>, int) throw();
    void set_message_verbosity(int) throw();
    int get_max_verbosity() const throw();
private:
    int max_verbosity;
    logstreambuf buf;
    vector<shared_ptr<ostream> > owned_streams;
};

inline int logstreambuf::overflow(int ch) {
    for (vector<tuple<int, streambuf *> >::iterator si = streams.begin();
         si != streams.end(); ++si) {
        if (msg_verb <= si->get<0>()) si->get<1>()->sputc(ch);
    }
    return 0;
}

inline void logstreambuf::set_message_verbosity(int v) throw() {
    msg_verb = v;
}

inline void logger::set_message_verbosity(int verb) throw() {
    buf.set_message_verbosity(verb);
}

inline int logger::get_max_verbosity() const throw() {
    return max_verbosity;
}

#define LOG(l,v) if ((v) <= ((l)).get_max_verbosity()) (l).set_message_verbosity((v)), (l)

#endif // __LOGGER_HPP__

