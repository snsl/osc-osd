# build everything

UTIL := ./util
TRGT := ./osd-target
INIT := ./osd_initiator
PVFS := ./pvfs
STGT := ./stgt

.PHONY: all init trgt stgt util clean pvfs

all: init trgt

init: util
	make -C $(INIT)
	make -C $(INIT)/tests/
	make -C $(INIT)/python

trgt: util stgt
	make -C $(TRGT)
	make -C $(TRGT)/tests

stgt:
	make -C $(STGT)

util:
	make -C $(UTIL)

clean:
	make -C $(UTIL) $@
	make -C $(STGT) $@
	make -C $(INIT)/python $@
	make -C $(INIT)/tests $@
	make -C $(INIT) $@
	make -C $(TRGT)/tests $@
	make -C $(TRGT) $@

# XXX: must configure first, though.  Generally do not do this
pvfs: util
	make -C $(PVFS) install

