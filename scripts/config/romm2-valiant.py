#!/usr/bin/env python2

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

def make_box_coords((src_x, src_y), (dst_x, dst_y)):
    assert (src_x,src_y) != (dst_x,dst_y)
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1
    return [(x, y) for x in range(src_x, dst_x + sx, sx)
                   for y in range(src_y, dst_y + sy, sy)]

def make_xy_coords((src_x, src_y), (dst_x, dst_y)):
    assert (src_x,src_y) != (dst_x,dst_y)
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1
    return ([(x, src_y) for x in range(src_x, dst_x + sx, sx)] +
            [(dst_x, y) for y in range(src_y + sy, dst_y + sy, sy)])

def make_xy_routes(flow, (src_x, src_y), (dst_x, dst_y),
                   set=None, num_vcs=None):
    coords = make_xy_coords((src_x,src_y), (dst_x,dst_y))
    return gen_full_routes(flow, coords, set=set, num_vcs=num_vcs)

def make_yx_coords((src_x, src_y), (dst_x, dst_y)):
    assert (src_x,src_y) != (dst_x,dst_y)
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1
    return ([(src_x, y) for y in range(src_y, dst_y + sy, sy)] +
            [(x, dst_y) for x in range(src_x + sx, dst_x + sx, sx)])

def make_yx_routes(flow, (src_x, src_y), (dst_x, dst_y),
                   set=None, num_vcs=None):
    coords = make_yx_coords((src_x,src_y), (dst_x,dst_y))
    return gen_full_routes(flow, coords, set=set, num_vcs=num_vcs)

def gen_full_routes(flow, coords0, set=None, num_vcs=None):
    assert len(coords0) > 1
    coords = [coords0[0]] + coords0 + [coords0[-1]]
    bridge = (flow, None, coords[0], get_cpu2net_vcs(num_vcs))
    inters = [(flow,prev,cur,[(next,1.0,None,get_vcs(cur,next,set,num_vcs))])
              for prev, cur, next in zip(coords, coords[1:], coords[2:])]
    return [bridge] + inters

def make_two_phase_routes(dims, src_id, dst_id, routing='xy',
                          stopover_coords=None, num_vcs=None):
    src = node_coords(dims, src_id)
    dst = node_coords(dims, dst_id)
    assert routing in ['xy', 'yx']
    if routing == 'xy':
        make_routes = make_xy_routes
    elif routing == 'yx':
        make_routes = make_yx_routes
    flow1 = ((src_id << 8) | dst_id) << 8
    flow2 = flow1 + 1
    def fix_stopover((f, _2, prev, _3),
                     (_4, _5, cur, old_next_hops)):
        next_hops = [(next, p, flow2, get_vcs(cur,next,1,num_vcs)) # VC set 1
                     for next, p, _, __ in old_next_hops]
        return f, prev, cur, next_hops
    def add_next_hop(nh_db, (next, prop, f, qs)):
        k = next, f, frozenset(qs)
        if k in nh_db: nh_db[k] += prop
        else: nh_db[k] = prop
    def add_route(db, route):
        for flow, prev, cur, nexts in route:
            k = flow, prev, cur
            if prev is None: # bridge route
                if k in db:
                    assert db[k] == nexts
                else:
                    db[k] = nexts
            else: # switch route
                if k not in db:
                    db[k] = {}
                for n in nexts:
                    add_next_hop(db[k], n)
    db = {}
    for stopover in stopover_coords(src, dst):
        if stopover == src:
            bridge, first = make_routes(flow1, src, dst, set=0,
                                        num_vcs=num_vcs)[:2]
            glue = fix_stopover(bridge, first)
            phase2 = make_routes(flow2, src, dst, set=1,
                                 num_vcs=num_vcs)[2:]
            route = [bridge, glue] + phase2
            add_route(db, route)
        elif stopover == dst:
            phase1 = make_routes(flow1, src, dst, set=0,
                                 num_vcs=num_vcs)[:-1]
            last = make_routes(flow2, src, dst, set=1,
                               num_vcs=num_vcs)[-1]
            glue = fix_stopover(phase1[-1], last)
            route = phase1 + [glue]
            add_route(db, route)
        else:
            phase1 = make_routes(flow1, src, stopover, set=0,
                                 num_vcs=num_vcs)[:-1]
            phase2_pre = make_routes(flow2, stopover, dst, set=1,
                                     num_vcs=num_vcs)[1:]
            glue = fix_stopover(phase1[-1], phase2_pre[0])
            phase2 = phase2_pre[1:]
            route = phase1 + [glue] + phase2
            add_route(db, route)
    def entry_for((flow, prev, cur)):
        nexts = db[(flow,prev,cur)]
        if prev is None: # bridge
            return (flow, prev, cur, nexts)
        else: # switch
            next_hops = [(nn, prop, nf, qs)
                         for (nn, nf, qs), prop in sorted(nexts.items())]
            return (flow, prev, cur, next_hops)
    return [entry_for(k) for k in sorted(db.keys())]

def make_romm2_routes(dims, src, dst, routing='xy', num_vcs=None):
    return make_two_phase_routes(dims, src, dst, routing=routing,
                                 stopover_coords=make_box_coords,
                                 num_vcs=num_vcs)

def make_valiant_routes(dims, src, dst, routing='xy', num_vcs=None):
    def make_all_coords(_1, _2):
        return make_box_coords((0,0), (dims[0]-1, dims[1]-1))
    return make_two_phase_routes(dims, src, dst, routing=routing,
                                 stopover_coords=make_all_coords,
                                 num_vcs=num_vcs)

def show_route(dims, (flow, prev, cur, rest)):
    def show_next_flow(f): return '' if f is None else '>0x%08x' % f
    if prev is None: # program bridge VCA
        return ('0x%08x@->0x%02x = %s' %
                (flow, node_id(dims,cur), ','.join(['%d' % q for q in rest])))
    else: # program node router/VCA
        ns = ['0x%02x%s@%g:%s' %
              (node_id(dims, n), show_next_flow(nf), p,
               ','.join(['%d' % q for q in qs]))
              for (n,p,nf,qs) in rest]
        return ('0x%08x@0x%02x->0x%02x = %s' %
                (flow, node_id(dims,prev), node_id(dims,cur), ' '.join(ns)))

def write_routes(type, (width,height), routing='xy', out=sys.stdout,
                 num_vcs=None):
    assert type in ['romm2', 'valiant']
    if type == 'romm2':
        make_routes = make_romm2_routes
    elif type == 'valiant':
        make_routes = make_valiant_routes
    print >>out, '\n[flows]'
    nodes = range(0, width*height)
    for src in nodes:
        for dst in nodes:
            if src != dst:
                print >>out
                print >>out, '# flow %02x -> %02x using %s' % (src,dst,type)
                for r in make_routes(dims, src, dst, routing=routing,
                                     num_vcs=num_vcs):
                    print >>out, show_route(dims, r)

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
    for type in ['romm2', 'valiant']:
        #for subtype in ['xy', 'yx']:
        for subtype in ['xy']:
            for oqpf in [False, True]:
                #for ofpq in [False, True]:
                for ofpq in [False]:
                    xvc=get_xvc_name(oqpf,ofpq)
                    for nvcs in [2,4,8]:
                        for bw in [1]:
                            #for mux in [None,1,2,4]:
                            #muxrngs = {2:[1,2], 4:[2,4], 8:[2,4] }
                            muxrngs = {2:[1], 4:[1], 8:[1] }
                            for mux in muxrngs[nvcs]:
                                # if mux and mux >= nvcs: continue
                                muxstr = 'mux%d' % mux if mux else 'nomux'
                                fn = '%s-%s-%s-vc%d-%s-bw%d.cfg' % (type, subtype, xvc, nvcs, muxstr, bw)
                                print 'writing %s...' % fn
                                with open(fn, 'w') as out:
                                    write_header(type, dims, bw=bw, cpubw=bw, mux=mux, num_vcs=nvcs,
                                                 oqpf=oqpf, ofpq=ofpq, out=out)
                                    write_routes(type, dims, routing=subtype, num_vcs=nvcs, out=out)
                    
