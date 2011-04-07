#!/usr/bin/env python

from __future__ import with_statement
import sys, string, operator

def get_cpu2net_vcs(num_vcs):
    assert num_vcs <= 8
    return range(0,num_vcs)
def get_net2cpu_vcs(num_vcs):
    assert num_vcs <= 8
    return range(8,8+num_vcs)

def concat(xs):
    return reduce(operator.add, xs, [])

# VC sets indexed by dst_coords - src_coords
vcs_table = {
    1: { (0,1): [[16],[]],   # -> VCs
         (0, -1): [[20],[]],  # <- VCs
         (1,0): [[24],[]],   # v VCs
         (-1,0): [[28],[]] },  # ^ VCs
    2: { (0,1): [[16],[18]],   # -> VCs
         (0, -1): [[20],[22]],  # <- VCs
         (1,0): [[24],[26]],   # v VCs
         (-1,0): [[28],[30]] },  # ^ VCs
    4: { (0,1): [[16,17],[18,19]],   # -> VCs
         (0, -1): [[20,21],[22,23]],  # <- VCs
         (1,0): [[24,25],[26,27]],   # v VCs
         (-1,0): [[28,29],[30,31]] },  # ^ VCs
    8: { (0,1): [[16,17,18,19],[20,21,22,23]],   # -> VCs
         (0, -1): [[24,25,26,27],[28,29,30,31]],  # <- VCs
         (1,0): [[32,33,34,35],[36,37,38,39]],   # v VCs
         (-1,0): [[40,41,42,43],[44,45,46,47]] }  # ^ VCs
}

def get_vcs((src_x,src_y), (dst_x,dst_y), set=None, num_vcs=None):
    assert num_vcs is not None
    assert num_vcs in [1,2,4,8]
    vcs = vcs_table[num_vcs]
    dx, dy = dst_x-src_x, dst_y-src_y
    if (dx,dy) == (0,0):
        return get_net2cpu_vcs(num_vcs)
    else:
        sets = vcs[(dx,dy)]
        return concat(sets) if set is None else sets[set]

def node_id((width,height), (x,y)):
    return x + width * y

def node_coords((width,height), id):
    return id % width, id / width

def make_xy_routes(flow, (src_x, src_y), (dst_x, dst_y), num_vcs):
    assert (src_x,src_y) != (dst_x,dst_y)
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1
    coords = ([(x, src_y) for x in range(src_x, dst_x + sx, sx)] +
              [(dst_x, y) for y in range(src_y + sy, dst_y + sy, sy)])
    return gen_full_routes(flow, coords, num_vcs=num_vcs)

def make_yx_routes(flow, (src_x, src_y), (dst_x, dst_y), num_vcs):
    assert (src_x,src_y) != (dst_x,dst_y)
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1
    coords = ([(src_x, y) for y in range(src_y, dst_y + sy, sy)] +
              [(x, dst_y) for x in range(src_x + sx, dst_x + sx, sx)])
    return gen_full_routes(flow, coords, num_vcs=num_vcs)

def make_o1turn_routes(flow, (src_x, src_y), (dst_x, dst_y), num_vcs):
    assert num_vcs >= 2
    assert (src_x,src_y) != (dst_x,dst_y)
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1

    coords_xy = ([(x, src_y) for x in range(src_x, dst_x + sx, sx)] +
                 [(dst_x, y) for y in range(src_y + sy, dst_y + sy, sy)])
    coords_yx = ([(src_x, y) for y in range(src_y, dst_y + sy, sy)] +
                 [(x, dst_y) for x in range(src_x + sx, dst_x + sx, sx)])
    if coords_xy == coords_yx: # all on X axis or all on Y axis
        return gen_full_routes(flow, coords_xy, None, num_vcs=num_vcs) # use all VCs
    else:
        xys = gen_full_routes(flow, coords_xy, 0, num_vcs=num_vcs) # use VC set 0
        yxs = gen_full_routes(flow, coords_yx, 1, num_vcs=num_vcs) # use VC set 1
        assert xys[0] == yxs[0] # bridge entry
        o1_0 = xys[0]
        flow0, prev0, cur0, ns0 = xys[1]
        flow1, prev1, cur1, ns1 = yxs[1]
        assert (flow0,prev0,cur0) == (flow1,prev1,cur1)
        o1_1 = (flow0, prev0, cur0, ns0 + ns1)
        return [o1_0, o1_1] + xys[2:] + yxs[2:]

def gen_full_routes(flow, coords0, set=None, num_vcs=None):
    assert num_vcs is not None
    assert len(coords0) > 1
    coords = [coords0[0]] + coords0 + [coords0[-1]]
    bridge = (flow, None, coords[0], get_cpu2net_vcs(num_vcs))
    inters = [(flow,prev,cur,[(next,1.0,get_vcs(cur,next,set,num_vcs))])
              for prev, cur, next in zip(coords, coords[1:], coords[2:])]
    return [bridge] + inters

def show_route(dims, (flow, prev, cur, rest)):
    if prev is None: # program bridge VCA
        return ('0x%06x@->0x%02x = %s' %
                (flow, node_id(dims,cur), ','.join(['%d' % q for q in rest])))
    else: # program node router/VCA
        ns = ['0x%02x@%g:%s' %
              (node_id(dims, n), p, ','.join(['%d' % q for q in qs]))
              for (n,p,qs) in rest]
        return ('0x%06x@0x%02x->0x%02x = %s' %
                (flow, node_id(dims,prev), node_id(dims,cur), ' '.join(ns)))

def write_route(type, dims, src_id, dst_id, num_vcs, out=sys.stdout):
    assert src_id != dst_id
    flow = ((src_id << 8) | dst_id) << 8
    src = node_coords(dims, src_id)
    dst = node_coords(dims, dst_id)
    if type == 'xy':
        routes = make_xy_routes(flow, src, dst, num_vcs=num_vcs)
    elif type == 'yx':
        routes = make_yx_routes(flow, src, dst, num_vcs=num_vcs)
    elif type == 'o1turn':
        routes = make_o1turn_routes(flow, src, dst, num_vcs=num_vcs)
    else:
        assert 'bad route type: %s' % type
    print >>out, '# flow %02x -> %02x using %s routing' % (src_id, dst_id, type)
    for r in routes:
        print >>out, show_route(dims, r)

def write_routes(type, (width,height), num_vcs, out=sys.stdout):
    print >>out, '\n[flows]'
    nodes = range(0, width*height)
    for src in nodes:
        for dst in nodes:
            if src != dst:
                write_route(type, (width,height), src, dst, num_vcs, out)

def write_header(type, (width, height), bw=None, cpubw=None, mux=None, num_vcs=None, oqpf=False, ofpq=False, out=sys.stdout):
    assert bw is not None
    assert cpubw is not None
    template = string.Template('''\
[geometry]
height = $height
width = $width

[routing]
node = weighted
queue = set
one queue per flow = $oqpf
one flow per queue = $ofpq

[node]
queue size = 8

[bandwidth]
cpu = $cpubw$mux
net = $cpubw
north = $bw$mux
east = $bw$mux
south = $bw$mux
west = $bw$mux

[queues]
cpu = $cpu2net
net = $net2cpu
north = $dn
east = $lt
south = $up
west = $rt

[core]
default = injector''')
    contents = template.substitute(width=width, height=height,
                                   bw=bw, cpubw=cpubw,
                                   mux=('/%d' % mux if mux else ''),
				   oqpf=str(oqpf).lower(),
				   ofpq=str(ofpq).lower(),
                                   net2cpu=' '.join(['%d' % q for q in get_net2cpu_vcs(num_vcs)]),
                                   cpu2net=' '.join(['%d' % q for q in get_cpu2net_vcs(num_vcs)]),
                                   lt=' '.join(['%d' % q for q in get_vcs((1,0),(0,0), num_vcs=num_vcs)]),
                                   rt=' '.join(['%d' % q for q in get_vcs((0,0),(1,0), num_vcs=num_vcs)]),
                                   up=' '.join(['%d' % q for q in get_vcs((0,1),(0,0), num_vcs=num_vcs)]),
                                   dn=' '.join(['%d' % q for q in get_vcs((0,0),(0,1), num_vcs=num_vcs)]))
    print >>out, contents

def get_xvc_name(oqpf, ofpq):
    xvc_names = { (False, False): 'std',
                  (True, True): 'xvc',
                  (False, True): '1fpq',
                  (True, False): '1qpf' }
    return xvc_names[(oqpf, ofpq)]

for dims in [(8,8)]:
    #for type in ['xy', 'yx', 'o1turn']:
    for type in ['xy', 'o1turn']:
        for oqpf in [False, True]:
            #for ofpq in [False, True]:
            for ofpq in [False]:
                xvc=get_xvc_name(oqpf,ofpq)
                for nvcs in [1,2,4,8]:#[1,2,4,8]
                    if type == 'o1turn' and nvcs == 1: continue
                    if oqpf and nvcs == 1: continue
                    for bw in [1]:#[1,2,4,8]
                        #for mux in [None,1,2,4]:
                        #muxrngs = { 1: [1], 2: [1,2], 4: [2,4], 8: [2,4] }
                        muxrngs = { 1: [1], 2: [1], 4: [1], 8: [1] }
                        for mux in muxrngs[nvcs]:
                            # if mux and mux >= nvcs: continue
                            muxstr = 'mux%d' % mux if mux else 'nomux'
                            fn = '%s-%s-vc%d-%s-bw%d.cfg' % (type, xvc, nvcs, muxstr, bw)
                            print 'writing %s...' % fn
                            with open(fn, 'w') as out:
                                write_header(type, dims, bw=bw, cpubw=16, mux=mux,
                                             num_vcs=nvcs, oqpf=oqpf,
                                             ofpq=ofpq, out=out)
                                write_routes(type, dims, nvcs, out=out)

