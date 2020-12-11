#!/usr/bin/env python2

from __future__ import with_statement
from matplotlib import rc
rc('font',**{'family':'sans-serif','sans-serif':['Helvetica']})
## for Palatino and other serif fonts use:
#rc('font',**{'family':'serif','serif':['Palatino']})
import matplotlib.pyplot as plot
import sys, re, glob
from matplotlib.backends.backend_pdf import PdfPages

cycles_re = re.compile(r'\s*total\s+statistics\s+cycles:\s+(\d+)\s+')
sent_re = re.compile(r'\s*all\s+flows\s+counts:\s+offered\s+(\d+),\s+sent\s+(\d+),\s+received\s+(\d+)\s+\((\d+) in flight\)\s*')
latency_re = re.compile(r'\s*all\s+flows\s+latency:\s+([\d.]+)\s+\+/-\s+([\d.]+)\s*')

def parse_log_file(fn):
    with open(fn, 'rU') as f:
        cycles, offered, sent, received = None, None, None, None
        lat_mean, lat_std = None, None
        for l in f:
            cycles_m = cycles_re.match(l)
            if cycles_m:
                cycles = int(cycles_m.group(1))
            sent_m = sent_re.match(l)
            if sent_m:
                offered, sent, received, flight = \
                    [int(sent_m.group(k)) for k in [1,2,3,4]]
            lat_m = latency_re.match(l)
            if lat_m:
                lat_mean, lat_std = [float(lat_m.group(k)) for k in [1,2]]
        if cycles and offered and received:
            offered_ps = float(offered)/float(cycles)
            received_ps = float(received)/float(cycles)
            frac_received = float(received)/float(offered)
            return (offered_ps, received_ps, lat_mean)
        else:
            return None

def get_dar_data(route, excl, vc, bw, mux, load, size):
    pat = ('output/%s-%s-%s-%s-%s-%s-%s-p*.out' %
           (load, size, route, excl, vc, mux, bw))
    fns = glob.glob(pat)
    if len(fns) == 0:
        print >>sys.stderr, ('WARNING: no stats files matching %s' % pat)
        return [], [], []
    raw_results = [parse_log_file(fn) for fn in fns]
    results = sorted([x for x in raw_results if x is not None])
    if len(results) > 0:
        ofrd, rcvd, lats = zip(*results)
        return list(ofrd), list(rcvd), list(lats)
    else:
        print >>sys.stderr, ('WARNING: no results in stats files matching %s' %
                             pat)
        return [], [], []

def make_plot(vc, bw, mux, load, size, pdf=None):
    plot_data = []
    file_root = '%s-%s-%s-%s-%s' % (load, size, vc, bw, mux)
    print 'plotting: %s...' % file_root
    for route in ['xy']:
        for excl in ['std', 'xvc', '1q1f', '1f1q']:
            name = '%s%s' % (route, '/' + excl if excl != 'std' else '')
            ofrd, rcvd, lats = get_dar_data(route,excl,vc,bw,mux,load,size)
            assert len(ofrd) == len(rcvd)
            if len(ofrd) > 0:
                plot_data.append((name, ofrd, rcvd, lats))
    if len(plot_data) > 0:
        plot.clf()
        _, xmax = plot.xlim()
        plot.xlim(0, xmax)
        plot.ylim(0,20)
        plot.title('%s %s %s %s %s' % (load, size, vc, bw, mux))
        plot.xlabel('offered traffic (flits/cycle)')
        plot.ylabel('received traffic (flits/cycle)')
        for name, ofrd, rcvd, lats in plot_data:
            assert len(ofrd) == len(rcvd)
            assert len(ofrd) > 0
            plot.plot(ofrd, rcvd, 'o-')
        plot.legend([name for name, ofrd, rcvd, lats in plot_data],
                    'lower right', shadow=True)
        if pdf is not None:
            pdf.savefig()
        else:
            plot.savefig(file_root + '.pdf')
        plot.close()

def main():
    pdf = PdfPages('darsim-xvc-plots.pdf')
    for load in ['bitcomp', 'transpose', 'shuffle']:
        for mux in ['mux1', 'nomux']:
            for vc in ['vc4', 'vc8']:
                for bw in ['bw1', 'bw2', 'bw4', 'bw8']:
                    for size in ['s2']:
                        make_plot(vc, bw, mux, load, size, pdf=pdf)
    pdf.close()

if __name__ == '__main__': main()

