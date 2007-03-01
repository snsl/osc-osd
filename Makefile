#build everything

all:
	make -C util
	make -C stgt
	make -C osd_initiator
	make -C osd_initiator/tests
	#~ make -C osd_initiator/python
	make -C osd-target
	make -C osd-target/tests

clean:
	make -C util $@
	make -C stgt $@
	make -C osd_initiator/python $@
	make -C osd_initiator/tests $@
	make -C osd_initiator $@
	make -C osd-target/tests $@
	make -C osd-target $@

