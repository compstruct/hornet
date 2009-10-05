// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <iterator>
#include <algorithm>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include "endian.hpp"
#include "version.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "sys.hpp"

using namespace std;
using namespace boost;
namespace po = boost::program_options;

static const string magic = "DAR ";
static const uint32_t version = 200907220;

typedef void (*custom_signal_handler_t)(int);

static logger syslog;
static shared_ptr<statistics> stats;

void sig_int_handler(int signo) {
    stats->end_sim();
    LOG(syslog,0) << endl << "simulation ended on keyboard interrupt" << endl
        << endl << *stats << endl;
    exit(1);
}

static uint32_t fresh_random_seed() {
    uint32_t random_seed = 0xdeadbeef;
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    urandom.read(reinterpret_cast<char *>(&random_seed), sizeof(random_seed));
    if (urandom.fail()) random_seed = time(NULL);
    return random_seed;
}

shared_ptr<sys> new_system(string file, uint64_t stats_start,
                           shared_ptr<vector<string> > events_files)
    throw(err) {
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
    shared_ptr<sys> s(new sys(img, stats_start, events_files, syslog));
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
         "simulate for arg cycles (default: forever)")
        ("stats-start", po::value<uint64_t>(),
         "start statistics after cycle arg (default: 0)")
        ("events", po::value<vector<string> >()->composing(),
         "read event schedule from file arg")
        ("log", po::value<vector<string> >()->composing(),
         "write a log to file arg")
        ("verbosity", po::value<int>(), "set console verbosity")
        ("log-verbosity", po::value<int>(), "set log verbosity")
        ("random-seed", po::value<uint32_t>(),
         "set random seed (default: use system entropy)")
        ("version", po::value<vector<bool> >()->zero_tokens()->composing(),
         "show program version and exit")
        ("help,h", po::value<vector<bool> >()->zero_tokens()->composing(),
         "show this help message and exit");
    hidden_opts_desc.add_options()
        ("mem_image", po::value<string>(), "memory image");
    args_desc.add("mem_image", 1);
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
    if (opts.count("mem_image") < 1) {
        cerr << "not enough arguments; try -h" << endl;
        exit(1);
    }
    if (opts.count("mem_image") > 1) {
        cerr << "too many arguments; try -h" << endl;
        exit(1);
    }
    string mem_image = opts["mem_image"].as<string>();
    int verb = (opts.count("verbosity") ?
                     opts["verbosity"].as<int>() : 0);
    syslog.add(cout, verb);
    if (opts.count("log")) {
        int log_verb = (opts.count("log-verbosity") ?
                             opts["log-verbosity"].as<int>() : verb);
        vector<string> fns = opts["log"].as<vector<string> >();
        for (vector<string>::const_iterator fn = fns.begin();
             fn != fns.end(); ++fn) {
            shared_ptr<ofstream> f(new ofstream(fn->c_str()));
            if (f->fail()) {
                cerr << "failed to write log: " << *fn << endl;
                exit(1);
            }
            syslog.add(f, log_verb);
        }
    }
    bool forever = true;
    uint64_t num_cycles = 0;
    uint64_t stats_start = 0;
    if (opts.count("cycles")) {
        forever = false;
        num_cycles = opts["cycles"].as<uint64_t>();
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
    if (opts.count("random-seed")) {
        srandom(opts["random-seed"].as<uint32_t>());
    } else {
        srandom(fresh_random_seed());
    }
    shared_ptr<vector<string> > events_files;
    if (opts.count("events")) {
        events_files = shared_ptr<vector<string> >(new vector<string>());
        vector<string> fns = opts["events"].as<vector<string> >();
        copy(fns.begin(), fns.end(),
             back_insert_iterator<vector<string> >(*events_files));
    }
    LOG(syslog,0) << dar_full_version << endl << endl;
    shared_ptr<sys> s;
    try {
        s = new_system(mem_image, stats_start, events_files);
    } catch (const err_parse &e) {
        cerr << e << endl;
        exit(1);
    } catch (const err &e) {
        cerr << "ERROR: " << e << endl;
        exit(1);
    }
    stats = s->get_statistics();
    custom_signal_handler_t prev_sig_int_handler =
        signal(SIGINT, sig_int_handler);
    if (prev_sig_int_handler == SIG_IGN) signal(SIGINT, prev_sig_int_handler);
    stats->start_sim();
    try {
        if (forever) {
            LOG(syslog,0) << "simulating forever" << endl;
        } else {
            LOG(syslog,0) << "simulating for " << dec << num_cycles
                << " cycle" << (num_cycles == 1 ? "" : "s") << endl;
        }
        LOG(syslog,0) << endl;
        for (unsigned cycle = 0; forever || cycle < num_cycles; ++cycle) {
            s->tick_positive_edge();
            s->tick_negative_edge();
        }
        stats->end_sim();
        LOG(syslog,0) << endl << "simulation ended successfully" << endl
            << endl << *stats << endl;
    } catch (const exc_syscall_exit &e) {
        stats->end_sim();
        LOG(syslog,0) << endl << "simulation ended on CPU exit()" << endl
            << endl << *stats << endl;
        exit(e.exit_code);
    } catch (const err &e) {
        cerr << "ERROR: " << e << endl;
        exit(2);
    }
    signal(SIGINT, prev_sig_int_handler);
}
