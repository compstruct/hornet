// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <cstdio>
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
                       const shared_ptr<ofstream> new_out,
                       uint64_t start, uint64_t end) throw(err)
    : time(new_time), start_time(start), end_time(end),
      last_timestamp(0), ids(), widths(),
      cur_values(), last_values(), dirty(), last_id(""), paths(),
      def_locs(), init_locs(), initialized(false), finalized(false),
      out(new_out) {
    assert(out);
}

void vcd_writer::finalize() throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    assert(!finalized);
    *out << endl;
    // erase definitions for signals that never changed
    for (stream_locs_t::const_iterator sli = def_locs.begin();
         sli != def_locs.end(); ++sli) {
        streampos start, end; tie(start,end) = sli->second;
        out->seekp(start);
        while (out->tellp() != end) {
            *out << ' ';
        }
    }
    // erase initial values for signals that never changed
    for (stream_locs_t::const_iterator sli = init_locs.begin();
         sli != init_locs.end(); ++sli) {
        streampos start, end; tie(start,end) = sli->second;
        out->seekp(start);
        while (out->tellp() != end) {
            *out << ' ';
        }
    }
    finalized = true;
}

string vcd_writer::get_fresh_id() throw() {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    for (string::reverse_iterator i = last_id.rbegin();
         i != last_id.rend(); ++i) {
        if (*i >= max_vcd_id_char) {
            *i = min_vcd_id_char;
        } else {
            ++(*i);
            return string(last_id);
        }
    }
    last_id = string(1, min_vcd_id_char) + last_id;
    return string(last_id);
}

void vcd_writer::new_signal(const vcd_id_t &id, const vector<string> &path,
                            unsigned width) throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    assert(!initialized);
    assert(!finalized);
    assert(id != 0);
    assert(path.size() > 0);
    assert(ids.find(id) == ids.end());
    assert(cur_values.find(id) == cur_values.end());
    assert(last_values.find(id) == last_values.end());
    ids[id] = get_fresh_id();
    widths[id] = width;
    last_values[id] = 0;
    insert_node(id, path, 0, paths);
}

void vcd_writer::write_val(const string &id, uint64_t val) throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    *out << 'b';
    if (val == 0) {
        *out << '0';
    } else {
        bool print_zeros = false;
        for (int i = 63; i >= 0; --i) {
            int d = (val >> i) & 1;
            if (d) {
                *out << '1';
                print_zeros = true;
            } else if (print_zeros) {
                *out << '0';
            }
        }
    }
    *out << ' ' << id;
}

void vcd_writer::writeln_val(const string &id, uint64_t val) throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    write_val(id, val);
    *out << '\n';
}

void vcd_writer::add_value(vcd_id_t id, uint64_t val) throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    if (is_sleeping()) return;
    assert(ids.find(id) != ids.end());
    assert(last_values.find(id) != last_values.end());
    cur_values_t::iterator cvi = cur_values.find(id);
    if (cvi == cur_values.end()) {
        cur_values[id] = val;
    } else {
        cvi->second += val;
    }
}

void vcd_writer::commit() throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    if (is_sleeping()) return;
    check_header();
    for (set<vcd_id_t>::iterator di = dirty.begin();
         di != dirty.end(); ++di) {
        if (cur_values.find(*di) == cur_values.end()) {
            check_timestamp();
            writeln_val(ids[*di], 0); // reset to default
            last_values[*di] = 0;
        }
    }
    dirty.clear();
    for (cur_values_t::iterator cvi = cur_values.begin();
         cvi != cur_values.end(); ++cvi) {
        last_values_t::iterator lvi = last_values.find(cvi->first);
        assert(lvi != last_values.end());
        if (cvi->second != lvi->second) {
            check_timestamp();
            writeln_val(ids[cvi->first], cvi->second);
            lvi->second = cvi->second;
            if (cvi->second != 0) {
                dirty.insert(cvi->first);
            }
            def_locs.erase(cvi->first);
            init_locs.erase(cvi->first);
        }
    }
    cur_values.clear();
}

void vcd_writer::declare_vars(const vcd_node::nodes_t &nodes,
                              const map<vcd_id_t, string> &ids) throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    assert(out);
    for (vcd_node::nodes_t::const_iterator ni = nodes.begin();
         ni != nodes.end(); ++ni) {
        if (ni->second.id != 0) {
            assert(ni->second.children.empty());
            map<vcd_id_t, string>::const_iterator ii = ids.find(ni->second.id);
            assert(ii != ids.end());
            assert(def_locs.find(ni->second.id) == def_locs.end());
            streampos start = out->tellp();
            *out << "$var integer 32 " << ii->second
                 << ' ' << ni->first << " $end";
            streampos end = out->tellp();
            *out << '\n';
            def_locs[ni->second.id] = make_tuple(start, end);
        } else {
            *out << "$scope module " << ni->first << " $end\n";
            declare_vars(ni->second.children, ids);
            *out << "$upscope $end\n";
        }
    }
}

void vcd_writer::check_header() throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    if (!initialized) {
        assert(out);
        assert(dirty.empty());
        assert(def_locs.empty());
        ptime now = second_clock::local_time();
        *out << "$date " << now << " $end\n"
             << "$version " << dar_full_version << " $end\n"
             << "$timescale 1ns $end\n";
        declare_vars(paths, ids);
        *out << "$enddefinitions $end\n";
        initialized = true;
        *out << '#' << dec << time << '\n';
        last_timestamp = time;
        *out << "$dumpvars\n";
        for (last_values_t::iterator lvi = last_values.begin();
             lvi != last_values.end(); ++lvi) {
            cur_values_t::iterator cvi = cur_values.find(lvi->first);
            if (cvi != cur_values.end()) {
                assert(cvi->first == lvi->first);
                writeln_val(ids[cvi->first], cvi->second);
                lvi->second = cvi->second;
                def_locs.erase(lvi->first);
                if (cvi->second != 0) {
                    dirty.insert(lvi->first);
                }
            } else {
                streampos start = out->tellp();
                write_val(ids[lvi->first], lvi->second);
                streampos end = out->tellp();
                *out << '\n';
                init_locs[lvi->first] = make_tuple(start, end);
            }
        }
        cur_values.clear();
        *out << "$end\n";
    }
}

void vcd_writer::check_timestamp() throw(err) {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    if (time > last_timestamp) {
        *out << '#' << dec << time << '\n';
        last_timestamp = time;
    }
}

bool vcd_writer::is_drained() const throw() {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    return cur_values.empty() && dirty.empty();
}

bool vcd_writer::is_sleeping() const throw() {
    unique_lock<recursive_mutex> lock(vcd_mutex);
    return time < start_time || (end_time != 0 && time > end_time);
}
