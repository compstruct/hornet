DARIMG=darimg
DARSIM=darsim

PROG = privateSharedMSI.img

.PHONY: all
all: $(PROG)

%.img: %.cfg 
	$(DARIMG) -o $@ $<

%.out: %.trace
	$(DARSIM) $(PROG) --concurrency 1 --memory-traces $< > $@

.PHONY: run
run: $(PROG) test_serialize.out test_hit.out test_share.out

.PHONY: clean
clean:
	rm *img 

