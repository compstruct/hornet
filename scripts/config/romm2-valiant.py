#!/usr/bin/env python

from __future__ import with_statement
import sys, string, operator

cpu2net_vcs = range(0,8)
net2cpu_vcs = range(8,16)

def concat(xs):
    return reduce(operator.add, xs, [])

# VC sets indexed by dst_coords - src_coords
vcs = { (0,1): [[16],[17]],   # -> VCs
        (0, -1): [[18],[19]],  # <- VCs
        (1,0): [[20],[21]],   # v VCs
        (-1,0): [[22],[23]] }  # ^ VCs

def get_vcs((src_x,src_y), (dst_x,dst_y), set=None):
    dx, dy = dst_x-src_x, dst_y-src_y
    if (dx,dy) == (0,0):
        return net2cpu_vcs
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

def make_xy_routes(flow, (src_x, src_y), (dst_x, dst_y), set=None):
    coords = make_xy_coords((src_x,src_y), (dst_x,dst_y))
    return gen_full_routes(flow, coords, set=set)

def make_yx_coords((src_x, src_y), (dst_x, dst_y)):
    assert (src_x,src_y) != (dst_x,dst_y)
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1
    return ([(src_x, y) for y in range(src_y, dst_y + sy, sy)] +
            [(x, dst_y) for x in range(src_x + sx, dst_x + sx, sx)])

def make_yx_routes(flow, (src_x, src_y), (dst_x, dst_y), set=None):
    coords = make_yx_coords((src_x,src_y), (dst_x,dst_y))
    return gen_full_routes(flow, coords, set=set)

def gen_full_routes(flow, coords0, set=None):
    assert len(coords0) > 1
    coords = [coords0[0]] + coords0 + [coords0[-1]]
    bridge = (flow, None, coords[0], cpu2net_vcs)
    inters = [(flow,prev,cur,[(next,1.0,None,get_vcs(cur,next,set))])
              for prev, cur, next in zip(coords, coords[1:], coords[2:])]
    return [bridge] + inters

def make_two_phase_routes(dims, src_id, dst_id, routing='xy',
                          stopover_coords=None):
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
        next_hops = [(next, p, flow2, get_vcs(cur,next,1)) # VC set 1
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
            bridge, first = make_routes(flow1, src, dst, set=0)[:2]
            glue = fix_stopover(bridge, first)
            phase2 = make_routes(flow2, src, dst, set=1)[2:]
            route = [bridge, glue] + phase2
            add_route(db, route)
        elif stopover == dst:
            phase1 = make_routes(flow1, src, dst, set=0)[:-1]
            last = make_routes(flow2, src, dst, set=1)[-1]
            glue = fix_stopover(phase1[-1], last)
            route = phase1 + [glue]
            add_route(db, route)
        else:
            phase1 = make_routes(flow1, src, stopover, set=0)[:-1]
            phase2_pre = make_routes(flow2, stopover, dst, set=1)[1:]
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

def make_romm2_routes(dims, src, dst, routing='xy'):
    return make_two_phase_routes(dims, src, dst, routing=routing,
                                 stopover_coords=make_box_coords)

def make_valiant_routes(dims, src, dst, routing='xy'):
    def make_all_coords(_1, _2):
        return make_box_coords((0,0), (dims[0]-1, dims[1]-1))
    return make_two_phase_routes(dims, src, dst, routing=routing,
                                 stopover_coords=make_all_coords)

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

def write_routes(type, (width,height), routing='xy', out=sys.stdout):
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
                for r in make_routes(dims, src, dst, routing=routing):
                    print >>out, show_route(dims, r)

def write_header((width, height), out=sys.stdout):
    template = string.Template('''\
[geometry]
height = $height
width = $width

[routing]
node = weighted
queue = set

[node]
queue size = 8

[bandwidth]
cpu = 8
net = 8
north = 2
east = 2
south = 2
west = 2

[queues]
cpu = $cpu2net
net = $net2cpu
north = $dn
east = $lt
south = $up
west = $rt

[code]
default = injector''')
    contents = template.substitute(width=width, height=height,
                                   net2cpu=' '.join(['%d' % q for q in net2cpu_vcs]),
                                   cpu2net=' '.join(['%d' % q for q in cpu2net_vcs]),
                                   lt=' '.join(['%d' % q for q in get_vcs((1,0),(0,0))]),
                                   rt=' '.join(['%d' % q for q in get_vcs((0,0),(1,0))]),
                                   up=' '.join(['%d' % q for q in get_vcs((0,1),(0,0))]),
                                   dn=' '.join(['%d' % q for q in get_vcs((0,0),(0,1))]))
    print >>out, contents

for dims in [(8,8)]:
    for type in ['romm2', 'valiant']:
        for subtype in ['xy', 'yx']:
            fn = '%dx%d-%s-%s.cfg' % (dims[0], dims[1], type, subtype)
            print 'writing %s...' % fn
            with open(fn, 'w') as out:
                write_header(dims, out)
                write_routes(type, dims, routing=subtype, out=out)

