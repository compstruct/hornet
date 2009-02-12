// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include "version.hpp"
#include "logger.hpp"
#include "sys.hpp"

using namespace std;
using namespace boost;
namespace po = boost::program_options;

static const string magic = "DAR ";
static const uint32_t version = 200902060;

shared_ptr<sys> new_system(string file, shared_ptr<logger> log) {
    shared_ptr<ifstream> img(new ifstream(file.c_str(), ios::in | ios::binary));
    if (img->fail()) {
        cerr << file << ": cannot read file" << endl;
        exit(1);
    }
    char file_magic[5] = { 0, 0, 0, 0, 0 };
    img->read(file_magic, 4);
    if (magic != file_magic) {
        cerr << file << ": not a DAR system image" << endl;
        exit(1);
    }
    uint32_t file_version = 0;
    img->read((char *) &file_version, 4);
    file_version = endian(file_version);
    if (file_version != version) {
        cerr << file << ": file format version (" << version << ") "
             << "does not match simulator (" << file_version << ")" << endl;
        exit(1);
    }
    shared_ptr<sys> s = shared_ptr<sys>(new sys(img, log));
    img->close();
    return s;
}


int main(int argc, char **argv) {
    po::options_description opts_desc("Options");
    po::options_description hidden_opts_desc("Hidden options");
    po::positional_options_description args_desc;
    po::options_description all_opts_desc;
    opts_desc.add_options()
        ("cycles", po::value<unsigned>(),
         "simulate for arg cycles (default: forever)")
        ("log", po::value<vector<string> >()->composing(),
         "write a log to the given file")
        ("verbosity", po::value<unsigned>(), "set console verbosity")
        ("log-verbosity", po::value<unsigned>(), "set log verbosity")
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
        cout << "not enough arguments; try -h" << endl;
        exit(1);
    }
    if (opts.count("mem_image") > 1) {
        cout << "too many arguments; try -h" << endl;
        exit(1);
    }
    string mem_image = opts["mem_image"].as<string>();
    shared_ptr<logger> log(new logger());
    unsigned verb = (opts.count("verbosity") ?
                     opts["verbosity"].as<unsigned>() : 0);
    log->add(cout, verbosity(verb));
    if (opts.count("log")) {
        unsigned log_verb = (opts.count("log-verbosity") ?
                             opts["log-verbosity"].as<unsigned>() : verb);
        vector<string> fns = opts["log"].as<vector<string> >();
        for (vector<string>::const_iterator fn = fns.begin();
             fn != fns.end(); ++fn) {
            shared_ptr<ofstream> f(new ofstream(fn->c_str()));
            if (f->fail()) {
                cerr << "failed to write log: " << *fn << endl;
                exit(1);
            }
            log->add(f, verbosity(log_verb));
        }
    }
    bool forever = true;
    unsigned num_cycles;
    if (opts.count("cycles")) {
        forever = false;
        num_cycles = opts["cycles"].as<unsigned>();
    }
    log << verbosity(1) << dar_full_version << endl;
    shared_ptr<sys> s;
    try {
        s = new_system(mem_image, log);
    } catch (const err &e) {
        cerr << "ERROR: " << e << endl;
        exit(1);
    }
    try {
        if (forever) {
            log << "simulating forever..." << endl;
        } else {
            log << "simulating for " << dec << num_cycles << " cycles..."
                << endl;
        }
        for (unsigned cycle = 0; forever || cycle < num_cycles; ++cycle) {
            s->tick_positive_edge();
            s->tick_negative_edge();
        }
    } catch (const exc_syscall_exit &e) {
        exit(e.exit_code);
    } catch (const err &e) {
        cerr << "ERROR: " << e << endl;
        exit(2);
    }
}

