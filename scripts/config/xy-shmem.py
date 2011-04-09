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
    base_flow = ((src << 8) | dst) << 8
    src_coord = node_coords(src)
    dst_coord = node_coords(dst)
    routes = []
    current_set = 0
    # memory messages
    for flow_prefix in range(num_vc_sets):
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
    global core_name, memory_name
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
bytes per flit = 8

[queues]
cpu = $cpu2net
net = $net2cpu
north = $down
east = $left
south = $up
west = $right

[core]
default = $core''')
    contents = template.substitute(x=width, y=height, qsize=num_flits_per_vc, 
                                   cpu2net_bw=from_cpu_bandwidth, net2cpu_bw=to_cpu_bandwidth, 
                                   link_bw=node_link_bandwidth, mux=num_xbar_input_per_node,
                                   core=core_name, 
                                   net2cpu=' '.join(['%d' % q for q in get_vcs('to_cpu')]),
                                   cpu2net=' '.join(['%d' % q for q in get_vcs('from_cpu')]),
                                   left=' '.join(['%d' % q for q in get_vcs((-1,0))]),
                                   right=' '.join(['%d' % q for q in get_vcs((1,0))]),
                                   up=' '.join(['%d' % q for q in get_vcs((0,-1))]),
                                   down=' '.join(['%d' % q for q in get_vcs((0,1))]))
    # core specific defaults
    if core_name == 'memtraceCore':
        contents = contents + '''\


[core::memtraceCore]
execution migration mode = never
message queue size = 4
migration context size in bytes = 128
maximum active threads per core = 2'''

    # memory defaults
    memory_fullname = {
        'privateSharedMSI':'private-shared MSI'
    }
    if memory_fullname.has_key(memory_name) is False:
        print 'Err - memory %s is not supported. must be one of %s'%(memory_name, str(memory_fullname.keys()))
        exit(-1)

    memoryTemplate = string.Template('''\


[memory]
architecture = $memory_name
dram controller location = top and bottom
core address translation = stripe
core address translation latency = 1
core address translation allocation unit in bytes = 4096
core address synch delay = 0
dram controller latency = 2
one-way offchip latency = 150
dram latency = 50
dram message header size in words = 4
maximum requests in flight per dram controller = 4
bandwidth in words per dram controller = 4''')
    contents = contents + memoryTemplate.substitute(memory_name=memory_fullname[memory_name])

    # memory specific defaults
    if memory_name in ['privateSharedMSI']:
        contents = contents + '''\


[memory::private-shared MSI]
words per cache line = 4
total lines in L1 = 32
associativity in L1 = 2 
hit test latency in L1 = 2
read ports in L1 = 2
write ports in L1 = 1
replacement policy in L1 = LRU
total lines in L2 = 128
associativity in L2 = 4 
hit test latency in L2 = 4
read ports in L2 = 2
write ports in L2 = 1
replacement policy in L2 = LRU'''


    print >>out, contents
    
def main(argv):
    # configurable parameters
    global width, height, num_vc_sets, num_vcs_per_set, num_flits_per_vc, core_name, memory_name
    # hidden parameters
    global to_cpu_bandwidth, from_cpu_bandwidth, node_link_bandwidth, num_xbar_input_per_node
    # automatically built on parameters 
    global to_cpu_vcs, from_cpu_vcs, node_link_vcs
    import getopt
    random.seed(7)

    # defaults
    width = 8
    height = 8
    num_vc_sets = 5       
    num_vcs_per_set = 1
    num_flits_per_vc = 4
    output_filename = 'output.cfg'
    to_cpu_bandwidth = 16
    from_cpu_bandwidth = 16
    node_link_bandwidth = 1
    num_xbar_input_per_node = 1 # using -to-1 mux
    core_name = 'memtraceCore'
    memory_name = 'privateSharedMSI'

    try:
        opts, args = getopt.getopt(argv,"x:y:v:q:c:o:n:m:") 
    except getopt.GetoptError:
        print 'Options'
        print '  -x <arg> network width(8)'
        print '  -y <arg> network height(8)'
        print '  -v <arg> number of VCs per set(1)'
        print '  -q <arg> maximum number of flits per VC(4)'
        print '  -c <arg> core type (memtraceCore)'
        print '  -m <arg> memory type (privateSharedMSI)'
        print '  -n <arg> number of VC sets'
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
        elif o == '-c':
            core_name = a
        elif o == '-m':
            memory_name = a
        elif o == '-n':
            num_vc_sets = int(a)
        elif o == '-o':
            output_filename = a
        else:
            print 'Wrong arguments'
            sys.exit(2)
        
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
    
