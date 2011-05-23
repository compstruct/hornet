DARIMG=darimg
DARSIM=darsim

CFG_FILES = privateSharedMSI.cfg privateSharedMESI.cfg \
            privateSharedLCCFixed10.cfg privateSharedLCCFixed10Save.cfg privateSharedLCCFixed10Rep.cfg \
            privateSharedLCCFixed20.cfg privateSharedLCCFixed20Save.cfg privateSharedLCCFixed20Rep.cfg \
            privateSharedLCCFixed30.cfg privateSharedLCCFixed30Save.cfg privateSharedLCCFixed30Rep.cfg \
            privateSharedLCCFixed40.cfg privateSharedLCCFixed40Save.cfg privateSharedLCCFixed40Rep.cfg \
            privateSharedLCCFixed50.cfg privateSharedLCCFixed50Save.cfg privateSharedLCCFixed50Rep.cfg \
            privateSharedLCCFixed100.cfg privateSharedLCCFixed100Save.cfg privateSharedLCCFixed100Rep.cfg \
            privateSharedLCCFixed200.cfg privateSharedLCCFixed200Save.cfg privateSharedLCCFixed200Rep.cfg \
            privateSharedLCCFixed500.cfg privateSharedLCCFixed500Save.cfg privateSharedLCCFixed500Rep.cfg \
            privateSharedLCCIdeal.cfg privateSharedLCCIdealSave.cfg privateSharedLCCIdealRep.cfg \
            privateSharedLCCZeroDelay.cfg privateSharedLCCZeroDelaySave.cfg privateSharedLCCZeroDelayRep.cfg 
CFG_NAMES = $(patsubst %.cfg, %, ${CFG_FILES})
IMG_FILES = $(patsubst %.cfg, %.img, ${CFG_FILES})
TRACE_FILES = test_serialize.trace test_hit.trace test_share_and_invalidate.trace test_inv_cache.trace random.trace
TRACE_NAMES = $(patsubst %.trace, %, ${TRACE_FILES})

.PHONY: all
all: $(IMG_FILES)

%.img: %.cfg 
	$(DARIMG) -o $@ $<

%.out: %.trace
	$(DARSIM) $(PROG) --concurrency 1 --memory-traces $< --random-seed 7 > $@

.PHONY: run
run: $(IMG_FILES) 
	$(foreach trace,$(TRACE_NAMES),$(foreach img,$(CFG_NAMES), $(DARSIM) $(img).img --concurrency 1 --memory-traces $(trace).trace --random-seed 23 > $(img)_$(trace).out;))

.PHONY: clean
clean:
	$(foreach trace, $(TRACE_NAMES), rm -f *$(trace).out;)
	$(foreach img,$(IMG_FILES), rm -f *$(img);)

