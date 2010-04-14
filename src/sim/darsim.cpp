// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <iterator>
#include <algorithm>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "endian.hpp"
#include "version.hpp"
#include "logger.hpp"
#include "vcd.hpp"
#include "statistics.hpp"
#include "sys.hpp"
#include "sim.hpp"

using namespace std;
using namespace boost;
using namespace boost::posix_time;
namespace po = boost::program_options;

static const string magic = "DAR ";
static const uint32_t version = 201003171;

typedef void (*custom_signal_handler_t)(int);

static logger syslog;
static shared_ptr<sys> s;
static shared_ptr<system_statistics> stats;
static shared_ptr<vcd_writer> vcd;
static bool report_stats = true;

custom_signal_handler_t prev_sig_int_handler;

void sig_int_handler(int signo) {
    signal(SIGINT, prev_sig_int_handler);
    if (vcd) vcd->commit();
    stats->end_sim();
    if (vcd) vcd->finalize();
    LOG(syslog,0) << endl << "simulation ended on keyboard interrupt" << endl;
    if (stats && report_stats) {
        LOG(syslog,0) << endl << *stats << endl;
    }
    exit(1);
}

static uint32_t fresh_random_seed() {
    uint32_t random_seed = 0xdeadbeef;
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    urandom.read(reinterpret_cast<char *>(&random_seed), sizeof(random_seed));
    if (urandom.fail()) random_seed = time(NULL);
    return random_seed;
}

shared_ptr<sys> new_system(const uint64_t &sys_time, string file,
                           uint64_t stats_start,
                           shared_ptr<vector<string> > evt_files,
                           uint32_t g_random_seed,
                           uint32_t test_flags) throw(err) {
    shared_ptr<ifstream> img(new ifstream(file.c_str(), ios::in | ios::binary));
    if (img->fail()) throw err_parse(file, "cannot read file");
    char file_magic[5] = { 0, 0, 0, 0, 0 };
    img->read(file_magic, 4);
    if (magic != file_magic) throw err_parse(file, "not a DAR system image");
    uint32_t file_version = 0;
    img->read((char *) &file_version, 4);
    file_version = endian(file_version);
    if (file_version != version) {
        ostringstream msg;
        msg << "file format version (" << dec << file_version << ") "
            << "does not match simulator (" << version << ")";
        throw err_parse(file, msg.str());
    }
    shared_ptr<sys> s(new sys(sys_time, img, stats_start,
                              evt_files, vcd, syslog, g_random_seed, false,
                              test_flags));
    img->close();
    return s;
}


int main(int argc, char **argv) {
    po::options_description opts_desc("Options");
    po::options_description hidden_opts_desc("Hidden options");
    po::positional_options_description args_desc;
    po::options_description all_opts_desc;
    opts_desc.add_options()
        ("cycles", po::value<uint64_t>(),
         "simulate for arg cycles (0 = until drained)")
        ("packets", po::value<uint64_t>(),
         "simulate until arg packets arrive (0 = until drained)")
        ("stats-start", po::value<uint64_t>(),
         "start statistics after cycle arg (default: 0)")
        //("stats-reset", po::value<uint64_t>(),
        // "reset statistics every arg cycles (default: never)")
        ("no-stats", po::value<vector<bool> >()->zero_tokens()->composing(),
         "do not report statistics")
        ("events", po::value<vector<string> >()->composing(),
         "read event schedule from file arg")
        ("log-file", po::value<vector<string> >()->composing(),
         "write a log to file arg")
        ("vcd-file", po::value<string>(),
         "write trace in VCD format to file arg")
        ("vcd-start", po::value<uint64_t>(),
         "start VCD dump at time arg (default: 0)")
        ("vcd-end", po::value<uint64_t>(),
         "end VCD dump at time arg (default: end of simulation)")
        ("verbosity", po::value<int>(), "set console verbosity")
        ("log-verbosity", po::value<int>(), "set log verbosity")
        ("random-seed", po::value<uint32_t>(),
         "set random seed (default: use system entropy)")
        ("concurrency", po::value<uint32_t>(),
         "simulator concurrency (default: automatic)")
        ("sync-period", po::value<uint64_t>(),
         "synchronize concurrent simulator every arg cycles\n(default: 0 = every posedge/negedge)")
        ("version", po::value<vector<bool> >()->zero_tokens()->composing(),
         "show program version and exit")
        ("help,h", po::value<vector<bool> >()->zero_tokens()->composing(),
         "show this help message and exit");
    hidden_opts_desc.add_options()
        ("mem-image", po::value<string>(), "memory image")
        ("test-flags", po::value<uint64_t>(), "test flags");
    args_desc.add("mem-image", 1);
    po::variables_map opts;
    all_opts_desc.add(opts_desc).add(hidden_opts_desc);
    try {
        po::store(po::command_line_parser(argc, argv).options(all_opts_desc).
                  positional(args_desc).run(), opts);
        po::notify(opts);
    } catch (po::error &e) {
        cerr << e.what() << endl;
        exit(1);
    }
    if (opts.count("help")) {
        cout << "Usage: darsim SYSTEM_IMAGE" << endl;
        cout << opts_desc;
    }
    if (opts.count("version")) cout << dar_full_version << endl;
    if (opts.count("version") || opts.count("help")) exit(0);
    if (opts.count("mem-image") < 1) {
        cerr << "not enough arguments; try -h" << endl;
        exit(1);
    }
    if (opts.count("mem-image") > 1) {
        cerr << "too many arguments; try -h" << endl;
        exit(1);
    }
    string mem_image = opts["mem-image"].as<string>();
    int verb = (opts.count("verbosity") ?
                     opts["verbosity"].as<int>() : 0);
    syslog.add(cout, verb);
    if (opts.count("log-file")) {
        int log_verb = (opts.count("log-verbosity") ?
                             opts["log-verbosity"].as<int>() : verb);
        vector<string> fns = opts["log-file"].as<vector<string> >();
        for (vector<string>::const_iterator fn = fns.begin();
             fn != fns.end(); ++fn) {
            shared_ptr<ofstream> f(new ofstream(fn->c_str()));
            if (f->fail()) {
                cerr << "ERROR: failed to write log: " << *fn << endl;
                exit(1);
            }
            syslog.add(f, log_verb);
        }
    }
    uint64_t num_cycles = 0;
    uint64_t num_packets = 0;
    uint64_t stats_start = 0;
    uint64_t stats_reset = 0;
    uint32_t g_random_seed = 0xdeadbeef;
    uint32_t concurrency = 0;
    if (opts.count("concurrency")) {
        concurrency = opts["concurrency"].as<uint32_t>();
        if (concurrency == 0) {
            cerr << "ERROR: --concurrency argument must be positive"
                 << endl;
            exit(1);
        }
    }
    uint64_t sync_period;
    if (opts.count("sync-period")) {
        sync_period = opts["sync-period"].as<uint64_t>();
    } else {
        sync_period = 0;
    }
    if (opts.count("cycles")) {
        num_cycles = opts["cycles"].as<uint64_t>();
    }
    if (opts.count("packets")) {
        num_packets = opts["packets"].as<uint64_t>();
    }
    if (opts.count("stats-start")) {
        stats_start = opts["stats-start"].as<uint64_t>();
        if (num_cycles != 0 && stats_start >= num_cycles) {
            cerr << "ERROR: statistics start (cycle " << stats_start
                 << ") after the simulation ends (cycle " << num_cycles
                 << ")" << endl;
            exit(1);
        }
    }
    if (opts.count("stats-reset")) {
        stats_reset = opts["stats-reset"].as<uint64_t>();
        if (stats_reset == 0) {
            cerr << "ERROR: statistics reset period must be positive" << endl;
            exit(1);
        }
    }
    if (opts.count("random-seed")) {
        srandom(opts["random-seed"].as<uint32_t>());
        g_random_seed = opts["random-seed"].as<uint32_t>();
    } else {
        srandom(fresh_random_seed());
        g_random_seed = fresh_random_seed();
    }
    shared_ptr<vector<string> > events_files;
    if (opts.count("events")) {
        events_files = shared_ptr<vector<string> >(new vector<string>());
        vector<string> fns = opts["events"].as<vector<string> >();
        copy(fns.begin(), fns.end(),
             back_insert_iterator<vector<string> >(*events_files));
    }
    if (opts.count("no-stats")) {
        report_stats = false;
    }
    LOG(syslog,0) << dar_full_version << endl << endl;
    vcd = shared_ptr<vcd_writer>();
    if (opts.count("vcd-file") == 1) {
        uint64_t vcd_start = 0, vcd_end = 0;
        string fn = opts["vcd-file"].as<string>();
        shared_ptr<ofstream> f(new ofstream(fn.c_str()));
        if (f->fail()) {
            cerr << "failed to write VCD: " << fn << endl;
            exit(1);
        }
        if (opts.count("vcd-start")) {
            vcd_start = opts["vcd-start"].as<uint64_t>();
            if (num_cycles != 0 && vcd_start >= num_cycles) {
                cerr << "ERROR: VCD start (cycle " << vcd_start
                    << ") after the simulation ends (cycle " << num_cycles
                    << ")" << endl;
                exit(1);
            }
        }
        if (opts.count("vcd-end")) {
            vcd_end = opts["vcd-end"].as<uint64_t>();
            if (vcd_end <= vcd_start) {
                cerr << "ERROR: VCD end (cycle " << vcd_end
                    << ") not after VCD start (cycle " << vcd_start
                    << ")" << endl;
                exit(1);
            } else if (num_cycles != 0 && vcd_end >= num_cycles) {
                cerr << "ERROR: VCD end (cycle " << vcd_end
                    << ") after the simulation ends (cycle " << num_cycles
                    << ")" << endl;
                exit(1);
            }
        }
        vcd = shared_ptr<vcd_writer>(new vcd_writer(0, f,
                                                    vcd_start, vcd_end));
    } else if (opts.count("vcd-file") > 1) {
        cerr << "ERROR: option --vcd-file admits only one argument" << endl;
        exit(1);
    } else if (opts.count("vcd-start") || opts.count("vcd-end")) {
        cerr << "ERROR: VCD dump start/end specified without VCD file" << endl;
        exit(1);
    }
    uint32_t test_flags = 0;
    if (opts.count("test-flags")) {
        test_flags = opts["test-flags"].as<uint64_t>();
    }
    try {
        s = new_system(0, mem_image, stats_start, events_files,
                       g_random_seed, test_flags);
        stats = s->get_statistics();
    } catch (const err_parse &e) {
        cerr << e << endl;
        exit(1);
    } catch (const err &e) {
        cerr << "ERROR: " << e << endl;
        exit(1);
    }
    prev_sig_int_handler = signal(SIGINT, sig_int_handler);
    if (prev_sig_int_handler == SIG_IGN) signal(SIGINT, prev_sig_int_handler);
    stats->start_sim();
    try {
        ptime sim_start_time = microsec_clock::local_time();
        uint32_t last_stats_start = stats_start;
        {
            // the_sim does not leave the scope until simulation ends
            sim the_sim(s, num_cycles, num_packets, sync_period, concurrency,
                        vcd, syslog);
        }
        ptime sim_end_time = microsec_clock::local_time();
        stats->end_sim();
        if (vcd) vcd->finalize();
        LOG(syslog,0) << endl << "simulation ended successfully" << endl;
        if (stats && report_stats) {
            if (last_stats_start < s->get_time()) {
                LOG(syslog,0) << "statistics for period [" << last_stats_start
                    << "," << s->get_time() << ")" << endl;
                LOG(syslog,0) << endl << *stats << endl;
            }
            time_duration sim_time = sim_end_time - sim_start_time;
            LOG(syslog,0) << endl;
            LOG(syslog,0) << "total simulation time:   "
                << dec << sim_time << endl
                << "total simulation cycles: "
                << dec << s->get_time() << endl
                << "total statistics cycles: "
                << dec << (s->get_time() - stats_start)
                << " (statistics start after cycle "
                << dec << stats_start
                << ")" << endl;
        }
    } catch (const exc_syscall_exit &e) {
        if (vcd) vcd->commit();
        stats->end_sim();
        if (vcd) vcd->finalize();
        LOG(syslog,0) << endl << "simulation ended on CPU exit()" << endl;
        if (stats && report_stats) {
            LOG(syslog,0) << endl << *stats << endl;
        }
        exit(e.exit_code);
    } catch (const err &e) {
        cerr << "ERROR: " << e << endl;
        exit(2);
    }
    signal(SIGINT, prev_sig_int_handler);
}
