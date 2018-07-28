// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __LOGGER_HPP__
#define __LOGGER_HPP__

#include <vector>
#include <iostream>
#include <memory>
#include <tuple>

using namespace std;

class logstreambuf : public streambuf {
public:
    logstreambuf();
    virtual ~logstreambuf();
    void add(streambuf *, int verbosity);
    void set_message_verbosity(int verbosity);
protected:
    virtual int overflow(int);
private:
    vector<std::tuple<int, streambuf *> > streams;
    int msg_verb; // current message verbosity
};

class logger : public ostream {
public:
    logger();
    virtual ~logger();
    void add(ostream &, int);
    void add(std::shared_ptr<ostream>, int);
    void set_message_verbosity(int);
    int get_max_verbosity() const;
private:
    int max_verbosity;
    logstreambuf buf;
    vector<std::shared_ptr<ostream> > owned_streams;
};

inline int logstreambuf::overflow(int ch) {
    for (vector<std::tuple<int, streambuf *> >::iterator si = streams.begin();
         si != streams.end(); ++si) {
        if (msg_verb <= get<0>(*si)) get<1>(*si)->sputc(ch);
    }
    return 0;
}

inline void logstreambuf::set_message_verbosity(int v) {
    msg_verb = v;
}

inline void logger::set_message_verbosity(int verb) {
    buf.set_message_verbosity(verb);
}

inline int logger::get_max_verbosity() const {
    return max_verbosity;
}

#define LOG(l,v) if ((v) <= ((l)).get_max_verbosity()) (l).set_message_verbosity((v)), (l)

#endif // __LOGGER_HPP__

