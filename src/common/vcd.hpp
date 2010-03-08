// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __VCD_HPP__
#define __VCD_HPP__

#include <string>
#include <vector>
#include <set>
#include <map>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread.hpp>
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
    vcd_writer(const uint64_t &time, const shared_ptr<ofstream> out,
               uint64_t start, uint64_t end) throw(err);
    void new_signal(const vcd_id_t &id, const vector<string> &path,
                    unsigned width) throw(err);
    void add_value(vcd_id_t id, uint64_t val) throw(err);
    void commit() throw(err); // call in the beginning of every tick
                              // to flush posedge values to VCD
    void finalize() throw(err); // call once at end to flush VCD
    bool is_drained() const throw();
private:
    typedef map<vcd_id_t, string> id_map_t;
    typedef map<vcd_id_t, unsigned> widths_t;
    typedef map<vcd_id_t, uint64_t> cur_values_t;
    typedef map<vcd_id_t, uint64_t> last_values_t;
    typedef map<vcd_id_t, tuple<streampos, streampos> > stream_locs_t;
private:
    string get_fresh_id() throw();
    void check_header() throw(err);
    void check_timestamp() throw(err);
    void write_val(const string &id, uint64_t val) throw(err);
    void writeln_val(const string &id, uint64_t val) throw(err);
    void declare_vars(const vcd_node::nodes_t &nodes,
                      const map<vcd_id_t, string> &ids) throw(err);
    bool is_sleeping() const throw();
private:
    const uint64_t &time;
    const uint64_t start_time;
    const uint64_t end_time;
    uint64_t last_timestamp;
    id_map_t ids;
    widths_t widths;
    cur_values_t cur_values;
    last_values_t last_values;
    set<vcd_id_t> dirty;
    string last_id;
    vcd_node::nodes_t paths;
    stream_locs_t def_locs;
    stream_locs_t init_locs;
    bool initialized;
    bool finalized;
    shared_ptr<ofstream> out;
    mutable recursive_mutex vcd_mutex;
};

#endif // __VCD_HPP__
