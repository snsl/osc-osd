#build everything

UTIL := ./util
TRGT := ./osd-target
INIT := ./osd_initiator
PVFS := ./pvfs
STGT := ./stgt

.PHONY: all trgt init pvfs

pvfs: init trgt
	make -C $(PVFS) install

init: trgt
	make -C $(INIT)
	make -C $(INIT)/tests/
	make -C $(INIT)/python

# builds util, stgt, osd-target, osd-target/tests/
trgt:  
	make -C $(TRGT) 

clean:
	make -C util $@
	make -C stgt $@
	make -C osd_initiator/python $@
	make -C osd_initiator/tests $@
	make -C osd_initiator $@
	make -C osd-target/tests $@
	make -C osd-target $@

