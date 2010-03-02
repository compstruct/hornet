// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ID_FACTORY__
#define __ID_FACTORY__

#include <string>
#include <cassert>
#include <boost/thread.hpp>

using namespace std;
using namespace boost;

template<class T>
class id_factory {
public:
    explicit id_factory(const string &name = "unknown") throw();
    T get_fresh_id() throw();
private:
    T next_id;
    mutex next_id_mutex;
    const string name;
private:
    id_factory(); // not defined
    id_factory(const id_factory &); // not defined
};

template<class T>
inline T id_factory<T>::get_fresh_id() throw() {
    unique_lock<mutex> lock(next_id_mutex);
    T id = next_id;
    ++next_id;
    return id;
}

template<class T>
inline id_factory<T>::id_factory(const string &nm) throw()
    : next_id(0), name(nm) { }

#endif // __ID_FACTORY__

