// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "version.hpp"
#include "vcd.hpp"

using namespace boost::posix_time;

const char min_vcd_id_char = '!';
const char max_vcd_id_char = '~';

static void insert_node(const vcd_id_t &id,
                        const vector<string> &path, unsigned path_index,
                        vcd_node::nodes_t &nodes) throw(err) {
    assert(path.size() > path_index);
    vcd_node &n = nodes[path[path_index]];
    if (path_index == path.size() - 1) { // leaf
        assert(n.id == 0); // otherwise duplicate signal
        n.id = id;
    } else { // node
        assert(n.id == 0); // otherwise unexpected leaf
        insert_node(id, path, path_index + 1, n.children);
    }
}

vcd_node::vcd_node() throw() : id(0), children() { }

vcd_writer::vcd_writer(const uint64_t &new_time,
                       const shared_ptr<ostream> new_out) throw(err)
    : time(new_time), last_timestamp(0), ids(), types(), last_values(),
      last_id(""), paths(), initialized(false), out(new_out) {
    assert(out);
}

vcd_writer::~vcd_writer() { }

string vcd_writer::get_fresh_id() throw() {
    for (string::reverse_iterator i = last_id.rbegin();
         i != last_id.rend(); ++i) {
        if (*i > max_vcd_id_char) {
            ++(*i);
            return string(last_id);
        } else {
            *i = min_vcd_id_char;
        }
    }
    last_id = string(1, min_vcd_id_char) + last_id;
    return string(last_id);
}

void vcd_writer::new_signal(const vcd_id_t &id, const vector<string> &path,
                            vcd_writer::val_t val_type) throw(err) {
    assert(id != 0);
    assert(path.size() > 0);
    assert(ids.find(id) == ids.end());
    assert(types.find(id) == types.end());
    assert(last_values.find(id) == last_values.end());
    ids[id] = get_fresh_id();
    types[id] = val_type;
    insert_node(id, path, 0, paths);
}

void vcd_writer::write_val(const string &id, vcd_writer::val_t val_type,
                           uint64_t val) throw(err) {
    if (val_type == vcd_writer::VCD_32 || val_type == vcd_writer::VCD_64) {
        *out << 'b';
        bool print_zeros = false;
        for (int i = (val_type == VCD_32 ? 31 : 63); i >= 0; --i) {
            int d = (val >> i) & 1;
            if (d) {
                *out << '1';
                print_zeros = true;
            } else if (print_zeros) {
                *out << '0';
            }
        }
        *out << ' ' << id << '\n';
    } else if (val_type == vcd_writer::VCD_REAL) {
        double d = (double) val;
        char s[32];
        snprintf(s, 32, "%.16g", d);
        *out << 'r' << s << ' ' << id << '\n';
    } else {
        assert(false);
    }
}

void vcd_writer::write_x(const string &id) throw(err) {
    *out << "bX " << id << '\n';
}

void vcd_writer::set_value_full(vcd_id_t id, vcd_writer::val_t type,
                                uint64_t val) throw(err) {
    assert(ids.find(id) != ids.end());
    assert(types.find(id) != types.end());
    assert(types[id] == type);
    check_header();
    last_values_t::iterator lvi = last_values.find(id);
    if (lvi == last_values.end() || lvi->second != val) {
        if (time > last_timestamp) {
            *out << '#' << dec << time << '\n';
            last_timestamp = time;
        }
        write_val(ids[id], type, val);
        last_values[id] = val;
    }
}

void vcd_writer::set_value(vcd_id_t id, uint32_t val) throw(err) {
    set_value_full(id, VCD_32, val);
}

void vcd_writer::set_value(vcd_id_t id, uint64_t val) throw(err) {
    set_value_full(id, VCD_64, val);
}

void vcd_writer::set_value(vcd_id_t id, double val) throw(err) {
    set_value_full(id, VCD_REAL, val);
}

void vcd_writer::unset(vcd_id_t id) throw(err) {
    assert(ids.find(id) != ids.end());
    assert(types.find(id) != types.end());
    check_header();
    write_x(ids[id]);
    last_values.erase(id);
}

static void declare_vars(shared_ptr<ostream> out,
                         const vcd_node::nodes_t &nodes,
                         const map<vcd_id_t, string> &ids) throw(err) {
    assert(out);
    for (vcd_node::nodes_t::const_iterator ni = nodes.begin();
         ni != nodes.end(); ++ni) {
        if (ni->second.id != 0) {
            assert(ni->second.children.empty());
            map<vcd_id_t, string>::const_iterator ii = ids.find(ni->second.id);
            assert(ii != ids.end());
            *out << "$var integer 32 " << ii->second
                 << ' ' << ni->first << " $end\n";
        } else {
            *out << "$scope module " << ni->first << " $end\n";
            declare_vars(out, ni->second.children, ids);
            *out << "$upscope $end\n";
        }
    }
}

void vcd_writer::check_header() throw(err) {
    if (!initialized) {
        assert(out);
        ptime now = second_clock::local_time();
        *out << "$date\n"
            << "    " << now << '\n'
            << "$end\n"
            << "$version\n"
            << "    " << dar_full_version << '\n'
            << "$end\n"
            << "$timescale 1ns $end\n";
        declare_vars(out, paths, ids);
        *out << "$enddefinitions $end\n";
        initialized = true;
        if (time == 0) {
            *out << "#0\n";
        }
    }
}
