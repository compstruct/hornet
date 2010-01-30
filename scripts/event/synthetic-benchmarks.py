#!/usr/bin/env python

from __future__ import with_statement
 
def num_bits(x):
    assert x > 1
    count = 0
    x -= 1
    while x:
        count += 1
        x >>= 1
    return count

def dst_of(mode, (width,height), src): # from Dally & Towles, p. 50
    assert width & (width-1) == 0    # require power of 2
    assert height & (height-1) == 0  # require power of 2
    nbits = num_bits(width) + num_bits(height)
    nbitsl, nbitsr = nbits//2, nbits - nbits//2
    mask = (1 << nbits) - 1
    maskl, maskr = (1 << nbitsl) - 1, (1 << nbitsr) - 1
    if mode == 'bitcomp':
        dst = ~src & mask
    elif mode == 'shuffle':
        dst = ((src >> (nbits-1)) & 1) | ((src << 1) & mask)
    elif mode == 'transpose':
        dst = ((src >> nbitsr) & maskl) | ((src & maskr) << nbitsr)
    elif mode == 'tornado':
        dstl = ((src >> nbitsr) + (-(nbitsl/-2) - 1)) & maskl
        dstr = (src + (-(nbitsr/-2) - 1)) & maskr
        dst = (dstl << nbitsr) | dstr
    elif mode == 'neighbor':
        dstl = ((src >> nbitsr) + 1) & maskl
        dstr = (src + 1) & maskr
        dst = (dstl << nbitsr) | dstr
    return dst

for dims in [(8,8)]:
    for mode in ['bitcomp', 'shuffle', 'transpose']: #, 'tornado', 'neighbor']:
        for size in [2, 8, 32, 64]:
            for period in range(1,21) + range(30,100,10):
                fn = '%s-s%d-p%d.evt' % (mode,size,period)
                print 'writing %s...' % fn
                with open(fn, 'w') as out:
                    for src in xrange(0, dims[0]*dims[1]):
                        dst = dst_of(mode, dims, src)
                        flow = ((src << 8) | dst) << 8;
                        if src != dst:
                            print >>out, ('flow 0x%06x size %d period %d' %
                                          (flow, size, period))
