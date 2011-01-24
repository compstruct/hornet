#!/usr/bin/env python

from __future__ import with_statement
import sys, string, operator
import random

vcs = { 'to_cpu' : [],
        'from_cpu' : [],
        (0,1): [],   
        (0, -1): [], 
        (1,0): [],  
        (-1,0): [] }  

def node_coords(id):
    global width
    return id % width, id / width

def node_id((x,y)):
    global width
    return x+width*y

def get_direction(src_coord, dst_coord):
    return tuple(map(lambda x,y: 1 if y>x else (-1 if y<x else 0), src_coord, dst_coord)) 

def get_vc_set(sets, set):
    global num_vc_sets
    if set is None:
        return sets
    else:
        assert (set < num_vc_sets)
        sets = sets[set*len(sets)/num_vc_sets:(set+1)*len(sets)/num_vc_sets]
        return sets
        
def get_vcs(direction, set=None):
    global vcs
    sets = vcs[direction]
    return get_vc_set(sets, set)

def traverse_x( flow, (from_x,from_y), (start_x,start_y), step, dist, set=None ):
    itin = [ (from_x,from_y), (start_x,start_y)]
    for i in range(0,dist):
        itin.append( (start_x+(i+1)*step,start_y) )
    p, c = 0, 1
    steps=[]
    for i in range(0,dist):
        steps.append( (flow, itin[p], itin[c], [(itin[c+1], None, 1, get_vcs(get_direction(itin[c],itin[c+1]),set) )] ) )
        p, c = p+1, c+1
    last = ( itin[c-1], itin[c] )
    return steps, last

def traverse_y( flow, (from_x,from_y), (start_x,start_y), step, dist, set=None ):
    itin = [ (from_x,from_y), (start_x,start_y)]
    for i in range(0,dist):
        itin.append( (start_x,start_y+(i+1)*step) )
    p, c = 0, 1
    steps=[]
    for i in range(0,dist):
        steps.append( (flow, itin[p], itin[c], [(itin[c+1], None, 1, get_vcs(get_direction(itin[c],itin[c+1]),set) )] ) )
        p, c = p+1, c+1
    last = ( itin[c-1], itin[c] )
    return steps, last
    
def make_xy_routes(flow, (src_x, src_y), (dst_x, dst_y), set=None):
    dx, dy = dst_x - src_x, dst_y - src_y
    sx, sy = 1 if src_x < dst_x else -1, 1 if src_y < dst_y else -1
    steps = []
    steps.append( (flow, None, (src_x,src_y), get_vcs('from_cpu', set) ) )
    steps_x, stop = traverse_x( flow, (src_x,src_y), (src_x,src_y), sx, sx*dx, set)
    steps_y, stop = traverse_y( flow, stop[0], stop[1], sy, sy*dy, set)
    steps = steps + steps_x + steps_y
    steps.append( (flow, stop[0], stop[1], [ (stop[1], None, 1, get_vcs('to_cpu', set) ) ] ) )
    return steps

def show_route((flow, prev, cur, rest)):
    if prev is None:
        return ('0x%08x@->0x%02x = %s' %
                (flow, node_id(cur), ','.join(['%d' % q for q in rest])))
    else:
        ns = []
        for (n,f,p,qs) in rest:
            if f is None:
                ns.append( '0x%02x@%g:%s' % (node_id(n), p, ','.join(['%d' % q for q in qs]) ) )
            else:
                ns.append( '0x%02x>0x%08x@%g:%s' % (node_id(n), f, p, ','.join(['%d' % q for q in qs]) ) )
        return ('0x%08x@0x%02x->0x%02x = %s' %
                (flow, node_id(prev), node_id(cur), ' '.join(ns)))

def write_route(src, dst, out):
    global do_em, do_ra
    base_flow = ((src << 8) | dst) << 8
    src_coord = node_coords(src)
    dst_coord = node_coords(dst)
    routes = []
    current_set = 0
    # memory messages
    for flow_prefix in [0, 1]:
        flow = base_flow | (flow_prefix << 24)
        routes = routes + make_xy_routes(flow, src_coord, dst_coord, current_set)
        current_set += 1
    # ra messages
    if do_ra:
        for flow_prefix in [2, 3]:
            flow = base_flow | (flow_prefix << 24)
            routes = routes + make_xy_routes(flow, src_coord, dst_coord, current_set)
            current_set += 1
    # em messages
    if do_em:
        for flow_prefix in [4, 5]:
            flow = base_flow | (flow_prefix << 24)
            routes = routes + make_xy_routes(flow, src_coord, dst_coord, current_set)
            current_set += 1
    print >>out, '# flow %02x -> %02x using xy routing' % (src, dst)
    for r in routes:
        print >>out, show_route(r)
    
def write_routes(out=sys.stdout):
    global width, height
    print >>out, '\n[flows]'
    nodes = range(0, width*height)
    for src in nodes:
        for dst in nodes:
            if src != dst:
                write_route(src, dst, out)

def write_header(out=sys.stdout):
    global width, height, num_flits_per_vc, from_cpu_bandwidth, to_cpu_bandwidth, node_link_bandwidth, num_xbar_input_per_node
    global core_name
    default_l1_locations = ''
    default_dram_ctrl_locations = ''
    for y in range(0, height):
        for x in range(0, width):
            default_l1_locations += '%d ' % (y*width + x)
            default_dram_ctrl_locations += '%d ' % (y*width if x < width/2 else y*width + (width-1)) 
    template = string.Template('''\
[geometry]
height = $x
width = $y

[routing]
node = weighted
queue = set

[node]
queue size = $qsize

[bandwidth]
cpu = $cpu2net_bw
net = $net2cpu_bw
north = $link_bw/$mux
east = $link_bw/$mux
south = $link_bw/$mux
west = $link_bw/$mux

[queues]
cpu = $cpu2net
net = $net2cpu
north = $down
east = $left
south = $up
west = $right

[core]

default = $core

[memory hierarchy]
1 = $l1_locs
2 = $dc_locs''')
    contents = template.substitute(x=width, y=height, qsize=num_flits_per_vc, 
                                   cpu2net_bw=from_cpu_bandwidth, net2cpu_bw=to_cpu_bandwidth, 
                                   link_bw=node_link_bandwidth, mux=num_xbar_input_per_node,
                                   core=core_name, l1_locs=default_l1_locations, dc_locs=default_dram_ctrl_locations,
                                   net2cpu=' '.join(['%d' % q for q in get_vcs('to_cpu')]),
                                   cpu2net=' '.join(['%d' % q for q in get_vcs('from_cpu')]),
                                   left=' '.join(['%d' % q for q in get_vcs((-1,0))]),
                                   right=' '.join(['%d' % q for q in get_vcs((1,0))]),
                                   up=' '.join(['%d' % q for q in get_vcs((0,-1))]),
                                   down=' '.join(['%d' % q for q in get_vcs((0,1))]))
    print >>out, contents
    
def main(argv):
    # configurable parameters
    global width, height, num_vc_sets, num_vcs_per_set, num_flits_per_vc, do_em, do_ra, core_name
    # hidden parameters
    global to_cpu_bandwidth, from_cpu_bandwidth, node_link_bandwidth, num_xbar_input_per_node
    # automatically built on parameters 
    global to_cpu_vcs, from_cpu_vcs, node_link_vcs
    import getopt
    random.seed(7)

    # defaults
    width = 8
    height = 8
    num_vc_sets = 2       # 1 for memory requests and 1 for memory replies
    num_vcs_per_set = 1
    num_flits_per_vc = 4
    do_em = False
    do_ra = False
    output_filename = 'output.cfg'
    to_cpu_bandwidth = 16
    from_cpu_bandwidth = 16
    node_link_bandwidth = 1
    num_xbar_input_per_node = 1 # using -to-1 mux
    core_name = 'memtraceCore'

    try:
        opts, args = getopt.getopt(argv,"x:y:v:q:ero:c:") 
    except getopt.GetoptError:
        print 'Options'
        print '  -x <arg> network width(8)'
        print '  -y <arg> network height(8)'
        print '  -v <arg> number of VCs per set(1)'
        print '  -q <arg> maximum number of flits per VC(4)'
        print '  -c <arg> core type (memtraceCore)'
        print '  -e support EM'
        print '  -r support RA'
        print '  -o <arg> output filename(output.cfg)'
        sys.exit(2)
    
    for o, a in opts:
        if o == '-x':
            width = int(a)
        elif o == '-y':
            height = int(a)
        elif o == '-v':
            num_vcs_per_set = int(a)
        elif o == '-q':
            num_flits_per_vc = int(a)
        elif o == '-e':
            do_em = True;
        elif 0 == '-c':
            core_name = a
        elif o == '-r':
            do_ra = True;
        elif o == '-o':
            output_filename = a
        else:
            print 'Wrong arguments'
            sys.exit(2)
        
    if do_em:
        num_vc_sets += 2
    if do_ra:
        num_vc_sets += 2
    num_vcs_per_link = num_vc_sets * num_vcs_per_set
    # for now, limit the total number of vcs to 32
    assert(num_vcs_per_link < 32)
    for i in range(0,num_vcs_per_link):
        # add from-CPU queues
        vcs['from_cpu'].append(i)
        vcs['to_cpu'].append(num_vcs_per_link*1+i)

    for i in range(0,num_vcs_per_link):
        vcs[(0,1)].append(num_vcs_per_link*2+i)
        vcs[(0,-1)].append(num_vcs_per_link*3+i)
        vcs[(1,0)].append(num_vcs_per_link*4+i)
        vcs[(-1,0)].append(num_vcs_per_link*5+i)

    print 'writing %s...' % output_filename
    
    with open(output_filename, 'w') as out:
        write_header(out)
        write_routes(out)

if __name__ == "__main__":
    main(sys.argv[1:])
    
