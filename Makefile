# build everything

util := ./osd-util
target := ./osd-target
initiator := ./osd-initiator
stgt := ./stgt
benchmarks := ./benchmarks

.PHONY: all initiator target stgt util benchmarks clean bmclean

all: initiator target

initiator: util
	make -C $(initiator)
	make -C $(initiator)/tests
	make -C $(initiator)/python

target: util
	make -C $(target)
	make -C $(target)/tests

stgt: target
	make -C $(stgt)

util:
	make -C $(util)

clean: bmclean
	make -C $(util) $@
	make -C $(stgt) $@
	make -C $(initiator)/python $@
	make -C $(initiator)/tests $@
	make -C $(initiator) $@
	make -C $(target)/tests $@
	make -C $(target) $@

ifneq (,$(wildcard $(benchmarks)))
benchmarks:
	make -C $(benchmarks)/osd-target
	make -C $(benchmarks)/pvfs/bonnie
	make -C $(benchmarks)/pvfs/metadata
	make -C $(benchmarks)/pvfs/perf

bmclean:
	make -C $(benchmarks)/osd-target clean
	make -C $(benchmarks)/pvfs/bonnie clean
	make -C $(benchmarks)/pvfs/metadata clean
	make -C $(benchmarks)/pvfs/perf clean
else
benchmarks:
bmclean:
endif

# distribution tarball, just makefile lumps
.PHONY: dist
MVD := osd-toplevel-$(shell date +%Y%m%d)
dist:
	tar cf - Makefile Makedefs | bzip2 -9c > $(MVD).tar.bz2

