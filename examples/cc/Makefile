DARIMG=darimg
DARSIM=darsim

PROG=privateSharedMSI.img

.PHONY: all
all: $(PROG)

%.img: %.cfg 
	$(DARIMG) -o $@ $<

.PHONY: run
run: $(PROG)
	$(DARSIM) $(PROG) --concurrency 1 --memory-traces test.trace > $<.out

.PHONY: clean
clean:
	rm *img 

