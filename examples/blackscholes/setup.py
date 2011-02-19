#!/usr/bin/env python

import string, struct, os, binascii

def binify(fin, fout):
	"""Convert blackscholes input files into a packed binary format that is easier 
	for mcpu.cpp to read."""
	sep = " "
	fin_h = open(fin, 'r')
	try: os.remove(fout) # files a stale file problem
	except OSError: pass # ... if file doesn't exist
	fout_h = open(fout, 'ab')	
	line_number = 0
	for line in fin_h.readlines():
		if line_number == 0:
			topack = struct.pack(	"i", int(line))
		else:
			li = tuple(string.split(line, sep))
			s, strike, r, divq, v, t, OptionType, divs, DGrefval = li
			topack = struct.pack(	"ffffffiff", \
														float(s), float(strike), float(r), float(divq), \
														float(v), float(t), int(OptionType), float(divs), \
														float(DGrefval))
		#print "L: " + str(line_number) + " is " + str(len(topack)) + " bytes."
		print "L: " + str(line_number) + ": " + binascii.hexlify(topack)		
		fout_h.write(topack)
		line_number += 1
	fout_h.close()
	fin_h.close()

binify("in_4K.txt", "in_4K.bin")
binify("in_16K.txt", "in_16K.bin")
