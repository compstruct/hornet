// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __VCD_HPP__
#define __VCD_HPP__

#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "cstdint.hpp"
#include "error.hpp"

using namespace std;
using namespace boost;

typedef const void *vcd_id_t;

class vcd_node {
public:
    vcd_node() throw();
public:
    typedef map<string, vcd_node> nodes_t;
public:
    vcd_id_t id; // invalid iff 0
    nodes_t children; // empty() <==> vcd_id == 0
};

class vcd_writer {
public:
    typedef enum { VCD_32, VCD_64, VCD_REAL } val_t;
public:
    vcd_writer(const uint64_t &time, const shared_ptr<ostream> out) throw(err);
    virtual ~vcd_writer();
    void new_signal(const vcd_id_t &id, const vector<string> &path,
                    val_t type) throw(err);
    void set_value(vcd_id_t id, uint32_t val) throw(err);
    void set_value(vcd_id_t id, uint64_t val) throw(err);
    void set_value(vcd_id_t id, double val) throw(err);
    void unset(vcd_id_t id) throw(err);
private:
    typedef map<vcd_id_t, string> id_map_t;
    typedef map<vcd_id_t, val_t> type_map_t;
    typedef map<vcd_id_t, uint64_t> last_values_t;
private:
    string get_fresh_id() throw();
    void check_header() throw(err);
    void write_val(const string &id, val_t type, uint64_t val) throw(err);
    void write_x(const string &id) throw(err);
    void set_value_full(vcd_id_t id, val_t type, uint64_t val) throw(err);
private:
    const uint64_t &time;
    uint64_t last_timestamp;
    id_map_t ids;
    type_map_t types;
    last_values_t last_values;
    string last_id;
    vcd_node::nodes_t paths;
    bool initialized;
    shared_ptr<ostream> out;
};

#define VCD_REGISTER(v, id, path, type) {if (v) v->new_signal(id, path, type);}

#define VCD_SET(v, id, new_val) {if (v) v->set_value(id, new_val);}

#endif // __VCD_HPP__
