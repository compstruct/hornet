// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ID_FACTORY__
#define __ID_FACTORY__

#include <string>
#include <cassert>

using namespace std;

template<class T>
class id_factory {
public:
    // template must leave a range of LSBs (~templ_mask) open for the actual ID
    explicit id_factory(const T &templ, const T &templ_mask);
    T get_first_id();
    T get_fresh_id();
private:
    T next_id;
    const T templ;
    const T templ_mask;
private:
    id_factory(); // not defined
    id_factory(const id_factory &); // not defined
};

template<class T>
inline id_factory<T>::id_factory(const T &new_templ,
                                 const T &new_templ_mask)
    : next_id(0), templ(new_templ), templ_mask(new_templ_mask) {
    assert((new_templ & new_templ_mask) == new_templ);
}

template<class T>
inline T id_factory<T>::get_first_id() {
    return templ;
}

template<class T>
inline T id_factory<T>::get_fresh_id() {
    assert((next_id & ~templ_mask) == next_id);
    T id = templ | next_id;
    ++next_id;
    return id;
}

#endif // __ID_FACTORY__

