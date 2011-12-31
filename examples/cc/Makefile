DARIMG=darimg
DARSIM=darsim

CFG_FILES = privateSharedMSIFT.cfg privateSharedMSIST.cfg privateSharedMESIFT.cfg privateSharedMESIST.cfg
CFG_NAMES = $(patsubst %.cfg, %, ${CFG_FILES})
IMG_FILES = $(patsubst %.cfg, %.img, ${CFG_FILES})
TRACE_FILES = test_serialize.trace test_hit.trace test_share_and_invalidate.trace test_inv_cache.trace 
TRACE_NAMES = $(patsubst %.trace, %, ${TRACE_FILES})

.PHONY: all
all: $(IMG_FILES)

%.img: %.cfg 
	$(DARIMG) -o $@ $<

%.out: %.trace
	$(DARSIM) $(PROG) --concurrency 1 --memory-traces $< --random-seed 7 > $@

.PHONY: test
test: $(IMG_FILES) 
	$(foreach trace,$(TRACE_NAMES),$(foreach img,$(CFG_NAMES), $(DARSIM) $(img).img --concurrency 1 --memory-traces $(trace).trace --random-seed 23 > $(img)_$(trace).out;))

.PHONY: clean
clean:
	$(foreach trace, $(TRACE_NAMES), rm -f *$(trace).out;)
	$(foreach img,$(IMG_FILES), rm -f *$(img);)

