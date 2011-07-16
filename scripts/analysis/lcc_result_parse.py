#!/usr/bin/env python

import sys, optparse

def main():
    usage = '%prog OUTPUT'
    opts_p = optparse.OptionParser(usage)
    opts, args = opts_p.parse_args()

    if len(args) != 1:
        exit
    try:
        f = open(args[0], 'r')
    except:
        print ''
        exit()
    total = 'n/a'
    reads = 'n/a'
    writes = 'n/a'
    aml = 'n/a'
    amrl = 'n/a'
    amwl = 'n/a'
    l1hit = 'n/a'
    l1rhit = 'n/a'
    l1whit = 'n/a'
    l2hit = 'n/a'
    l2rhit = 'n/a'
    l2whit = 'n/a'
    cathit = 'n/a'
    l1ops = 'n/a'
    l2ops = 'n/a'
    blks = 'n/a'
    blks_evict = 'n/a'
    invs = 'n/a'
    inv_tgts = 'n/a'
    inv_cycles = 'n/a'
    cost_mem_s = 'n/a'
    cost_l1_ns = 'n/a'
    cost_l1_act = 'n/a'
    cost_l1_evt = 'n/a'
    cost_cat_s = 'n/a'
    cost_cat_act = 'n/a'
    cost_l2_ns = 'n/a'
    cost_l2_blk = 'n/a'
    cost_l2_inv = 'n/a'
    cost_l2_act = 'n/a'
    cost_dram_ns = 'n/a'
    cost_dram_off = 'n/a'

    mig_rate = 'n/a'
    total_migs = 'n/a'
    total_in_migs = 'n/a'
    total_th_evicts = 'n/a'
    total_mig_lat = 'n/a'
    total_in_mig_lat = 'n/a'
    total_th_evict_lat = 'n/a'

    shreq = 'n/a'
    exreq = 'n/a'
    invrep = 'n/a'
    invrep_on_req = 'n/a'
    flushrep = 'n/a'
    flushrep_on_req = 'n/a'
    wbrep = 'n/a'
    wbrep_on_req = 'n/a'
    shrep = 'n/a'
    exrep = 'n/a'
    invreq = 'n/a'
    wbreq = 'n/a'
    flushreq = 'n/a'
    i_to_s = 'n/a'
    i_to_e = 'n/a'
    s_to_s = 'n/a'
    s_to_e = 'n/a'
    e_to_s = 'n/a'
    e_to_e = 'n/a'

    for line in f:
        if line.find('[Summary: Thread') == 0:
            words = line.split()
            reads = words[6]
            writes = words[8]
            total = str(int(reads)+int(writes))
            aml = words[10]
            amrl = words[12]
            amwl = words[14]
            try:
                mig_rate = words[16]
                total_migs = words[19]
                total_in_migs = words[21]
                total_th_evicts = words[23]
                total_mig_lat = words[27]
                total_in_mig_lat = words[30]
                total_th_evict_lat = words[33]
            except:
                # legacy
                mig_rate = '0'
                total_migs = '0'
                total_in_migs = '0'
                total_th_evicts = '0'
                total_mig_lat = '0'
                total_in_mig_lat = '0'
                total_th_evict_lat = '0'
        elif line.find('[Summary: Private-shared-LCC') == 0:
            words = line.split()
            l1hit = words[8]
            l1rhit = l1hit
            l1whit = '0'
            l2hit = words[10]
            l2rhit = words[12]
            l2whit = words[14]
            blks = words[16]
            blks_evict = words[18][:-1]
            cathit = words[20]
            l1ops = words[22]
            l2ops = words[24]
            invs = '0'
            inv_tgts = '0'
            inv_cycles = '0'
        elif line.find('[Summary: Private-shared-MSI') == 0:
            words = line.split()
            l1hit = words[8]
            l1rhit = words[10]
            l1whit = words[12]
            l2hit = words[14]
            l2rhit = 'n/a'
            l2whit = 'n/a'
            blks = '0'
            blks_evict = '0'
            invs = words[16]
            inv_tgts = words[18]
            inv_cycles = words[20]
            cathit = words[22]
            l1ops = words[24]
            l2ops = words[26]
        elif line.find('[Summary: Private-shared-EMRA') == 0:
            words = line.split()
            l1hit = words[8]
            l1rhit = words[10]
            l1whit = words[12]
            l2hit = words[18]
            l2rhit = words[20]
            l2whit = words[22]
            blks = '0'
            blks_evict = '0'
            invs = '0'
            inv_tgts = '0'
            inv_cycles = '0'
            cathit = words[24]
            l1ops = words[26]
            l2ops = words[28]
        elif line.find('[Latency Breakdown ') == 0:
            words = line.split()
            if words[9] == 'L1-evict:':
                cost_mem_s = words[4]
                cost_l1_ns = words[6]
                cost_l1_act = words[8]
                cost_l1_evt = words[10]
                cost_cat_s = words[12]
                cost_cat_act = words[14]
                cost_l2_ns = words[16]
                cost_l2_inv = words[18]
                cost_l2_act = words[20]
                cost_dram_ns = words[22]
                cost_dram_off = words[24]
                cost_l2_blk = '0'
            else:
                cost_mem_s = words[4]
                cost_l1_ns = words[6]
                cost_l1_act = words[8]
                cost_l1_evt = '0'
                cost_cat_s = words[10]
                cost_cat_act = words[12]
                cost_l2_ns = words[14]
                cost_l2_inv = '0'
                cost_l2_act = words[16]
                cost_dram_ns = words[18]
                cost_dram_off = words[20]
        elif line.find('[Coherence Messages 1') == 0:
            words = line.split()
            shreq = words[5]
            exreq = words[7]
            invrep = words[9]
            invrep_on_req = words[12][:-1]
            flushrep = words[14]
            flushrep_on_req = words[17][:-1]
            wbrep = words[19]
            wbrep_on_req = words[22][:-1]
        elif line.find('[Coherence Messages 2') == 0:
            words = line.split()
            shrep = words[5]
            exrep = words[7]
            invreq = words[9]
            wbreq = words[13]
            flushreq = words[17]
        elif line.find('[State Transitions on') == 0:
            words = line.split()
            i_to_s = words[6]
            i_to_e = words[8]
            s_to_s = words[10]
            s_to_e = words[12]
            e_to_s = words[14]
            e_to_e = words[16]
    print total + ' ' + reads + ' ' + writes + ' ' + aml + ' ' + amrl + ' ' + amwl + ' ' + \
          l1hit + ' ' + l1rhit + ' ' + l1whit + ' ' + l2hit + ' ' + l2rhit + ' ' + l2whit + ' ' + \
          cathit + ' ' + l1ops + ' ' + l2ops + ' ' + \
          invs + ' ' + inv_tgts + ' ' + inv_cycles + ' ' +\
          blks + ' ' + blks_evict + ' ' +\
          mig_rate + ' ' + total_migs + ' ' + total_in_migs + ' ' + total_th_evicts + ' ' +\
          total_mig_lat + ' ' + total_in_mig_lat + ' ' + total_th_evict_lat + ' ' +\
          cost_mem_s + ' ' + ' ' + cost_cat_s + ' ' + cost_cat_act + ' ' + \
          cost_l1_ns + ' ' + cost_l1_act + ' ' + cost_l1_evt + ' ' + \
          cost_l2_ns + ' ' + cost_l2_act + ' ' + cost_l2_inv + ' ' + cost_l2_blk + ' ' + \
          cost_dram_ns + ' ' + cost_dram_off + ' ' + total_in_mig_lat + ' ' +\
          i_to_s + ' ' + i_to_e + ' ' + s_to_s + ' ' + s_to_e + ' ' + e_to_s + ' ' + e_to_e + ' ' + \
          shreq + ' ' + exreq + ' ' + invrep + ' ' + invrep_on_req + ' ' + flushrep + ' ' + flushrep_on_req + ' ' + \
          wbrep + ' ' + wbrep_on_req + ' ' + \
          shrep + ' ' + exrep + ' ' + invreq + ' ' + wbreq + ' ' + flushreq

if __name__ == '__main__': main()

