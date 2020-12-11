#!/usr/bin/env python2

from __future__ import with_statement
import sys, string, operator
import random
    
stripe_unit = 0x1ff

def truncate(a, b=100):
    if a < 0:
        print '[WARNING] a value set to min(0)'
        return 0
    elif a > b:
        print '[WARNING] a value set to max(' + str(b) + ')'
        return b
    else:
        return a

def main(argv):
    global stripe_unit
    import getopt
    random.seed(7)

    # defaults
    num_threads = 64
    mem_ratio = 20
    write_ratio = 25
    thread_length = 10000
    private_ratio = 70
    temporal_locality = 40
    out_file = 'synthetic.memtrace'

    try:
        opts, args = getopt.getopt(argv,"n:m:w:l:p:t:")
    except getopt.GetoptError:
        print 'Options (defaults)'
        print ' -n <arg> number of threads (64)'
        print ' -l <arg> number of instructions per thread (10000)'
        print ' -m <arg> % of memory instructions (20)'
        print ' -w <arg> % of writes in memory instructions (25)'
        print ' -p <arg> % of private data accesses in memory instructions (70)'
        print ' -t <arg> temporal locality index [0 to 99] (40)'
        print ' -o <arg> output filename (synthetic.memtrace)'
        sys.exit(2)
    
    for o, a in opts:
        if o == '-n':
            num_threads = int(a)
        elif o == '-l':
            thread_length = int(a)
        elif o == '-m':
            mem_ratio = truncate(int(a))
        elif o == '-w':
            write_ratio = truncate(int(a))
        elif o == '-p':
            private_ratio = truncate(int(a))
        elif o == '-t':
            temporal_locality = truncate(int(a), 99)
        elif o == '-o':
            out_file = a
        else:
            print 'Wrong arguments'
            sys.exit(2)
        
    mem_interval = 100 / mem_ratio

    if private_ratio == 0 :
        prob_local_to_remote = 100
        prob_remote_to_local = 0
    elif private_ratio == 100:
        prob_local_to_remote = 100
        prob_remote_to_local = 0
    else:
        max_prob_local_to_remote = min(100, 100*(100-private_ratio)/private_ratio)
        prob_local_to_remote = max(1, max_prob_local_to_remote * (100-temporal_locality) / 100)
        prob_remote_to_local = max(1, prob_local_to_remote * private_ratio / (100 - private_ratio))

    print ' - memory instructions in every ' + str(mem_interval) + ' non-memory instructions.'
    print ' - actual private access ratio : ' + str( float(prob_remote_to_local) / float(prob_remote_to_local + prob_local_to_remote) * 100) + ' %'
    print '     - the probability of accessing remote data after accessing a local data : ' + str(prob_local_to_remote) + ' %'
    print '     - the probability of accessing local data after accessing a local data : ' + str(prob_local_to_remote) + ' %'
    with open(out_file, 'w') as out:
        for thread in range(num_threads):
            local = True
            for i in range(thread_length * mem_ratio / 100):
                r = random.randrange(100)
                if local and r < prob_local_to_remote:
                    local = False
                elif not local and r < prob_remote_to_local:
                    local = True
                if local:
                    home = thread
                else:
                    r = random.randrange(63)
                    home = r if r != thread else 63
                r = random.randrange(stripe_unit>>2) << 2
                addr = home * (stripe_unit+1) + r
                rw = 'W ' if random.randrange(100) < write_ratio else 'R '
                print >>out, 'Thread ' + str(thread) + ' ' + hex(addr)[2:] + ' ' + rw + 'deadbeef ' + hex(home)[2:] + ' ' + str(mem_interval)

if __name__ == "__main__":
    main(sys.argv[1:])
    
