# build everything

util := ./osd-util
target := ./osd-target
initiator := ./osd-initiator
stgt := ./stgt
benchmarks := ./benchmarks

.PHONY: all initiator target stgt util benchmarks clean bmclean

all: initiator target stgt

initiator: util
	$(MAKE) -C $(initiator)
	$(MAKE) -C $(initiator)/tests
	$(MAKE) -C $(initiator)/python

target: util
	$(MAKE) -C $(target)
	$(MAKE) -C $(target)/tests

util:
	$(MAKE) -C $(util)

clean: bmclean stgtclean
	$(MAKE) -C $(initiator) $@
	$(MAKE) -C $(initiator)/tests $@
	$(MAKE) -C $(initiator)/python $@
	$(MAKE) -C $(target) $@
	$(MAKE) -C $(target)/tests $@
	$(MAKE) -C $(util) $@

ifneq (,$(wildcard $(stgt)))
stgt: target
	$(MAKE) -C $(stgt)

stgtclean:
	$(MAKE) -C $(stgt) clean
else
stgt:
stgtclean:
endif

ifneq (,$(wildcard $(benchmarks)))
benchmarks:
	$(MAKE) -C $(benchmarks)/osd-target
	$(MAKE) -C $(benchmarks)/pvfs/bonnie
	$(MAKE) -C $(benchmarks)/pvfs/metadata
	$(MAKE) -C $(benchmarks)/pvfs/perf

bmclean:
	$(MAKE) -C $(benchmarks)/osd-target clean
	$(MAKE) -C $(benchmarks)/pvfs/bonnie clean
	$(MAKE) -C $(benchmarks)/pvfs/metadata clean
	$(MAKE) -C $(benchmarks)/pvfs/perf clean
else
benchmarks:
bmclean:
endif

# distribution tarball, just makefile lumps
.PHONY: dist
MVD := osd-toplevel-$(shell date +%Y%m%d)
dist:
	tar cf - Makefile Makedefs | bzip2 -9c > $(MVD).tar.bz2

