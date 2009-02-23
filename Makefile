# Top level Makefile to build everything

MK_PATH ?= .
util := $(MK_PATH)/osd-util
tgt := $(MK_PATH)/tgt
target := $(MK_PATH)/osd-target
initiator := $(MK_PATH)/osd-initiator
CHECKPATCH ?= ~/dev/git/pub/linux/scripts/checkpatch.pl
checkpatch_2_kdev = checkpatch-2-kdev

.PHONY: all clean
# comment out any below to remove from compelation
all: target
all: stgt
# FreeBSD does not build these yet
UNAME=$(shell uname)
ifneq (FreeBSD,$(UNAME))
all: target_test
all: initiator
all: initiator_tests
all: initiator_python
endif

clean: stgt_clean initiator_clean target_clean util_clean

.PHONY: util util_clean
util:
	$(MAKE) -C $(util)

util_clean:
	$(MAKE) -C $(util) clean

.PHONY: target target_test target_clean
target: util
	$(MAKE) -C $(target)

target_test: target
	$(MAKE) -C $(target)/tests

target_clean:
	$(MAKE) -C $(target) clean
	$(MAKE) -C $(target)/tests clean

.PHONY: initiator initiator_tests initiator_python initiator_clean
initiator: util
	$(MAKE) -C $(initiator)

initiator_tests: initiator
	$(MAKE) -C $(initiator)/tests

initiator_python: initiator
	$(MAKE) -C $(initiator)/python

initiator_clean:
	$(MAKE) -C $(initiator)/python clean
	$(MAKE) -C $(initiator)/tests clean
	$(MAKE) -C $(initiator) clean

.PHONY: stgt stgt_checkpatch stgt_tgt_only stgt_clean
stgt: target
	$(MAKE) ISCSI=1 -C $(tgt)/usr

stgt_checkpatch:
	cd $(tgt);git-show | $(CHECKPATCH) - |  $(checkpatch_2_kdev) $(PWD)/$(tgt)

stgt_tgt_only:
	$(MAKE) ISCSI=1 IBMVIO=1 ISCSI_RDMA=1 FCP=1 FCOE=1 OSDEMU=1 -C $(tgt)/usr

stgt_clean:
	$(MAKE) -C $(tgt)/usr clean

# distribution tarball, just makefile lumps
.PHONY: dist osd_checkpatch
MVD := osd-toplevel-$(shell date +%Y%m%d)
dist:
	tar cf - Makefile Makedefs | bzip2 -9c > $(MVD).tar.bz2

# ctags generation for all projects
ctags:
	ctags -R $(util) $(target) $(initiator) $(tgt) /usr/include

osd_checkpatch:
	git-show | $(CHECKPATCH) - | $(checkpatch_2_kdev) $(PWD)

# ==== Remote compelation ================================================
# make R=remote_machine R_PATH=source_path_on_remote remote
R ?= "perf-x4"
R_PATH ?= "/fs/home/bharrosh/osc-osd"

.PHONY: remote remote_clean
remote:
	ssh $(R) "cd $(R_PATH);gmake MK_PATH=$(R_PATH) -C $(R_PATH)"

remote_clean:
	ssh $(R) "cd $(R_PATH);gmake MK_PATH=$(R_PATH) -C $(R_PATH) clean"
