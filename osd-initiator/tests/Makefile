#
# OSD initiator tests
#

PROGSRC := sgio.c generic_iface_test.c iovec.c iospeed.c latency.c atomic.c
PROGSRC += blk-iospeed.c
PROGOBJ := $(PROGSRC:.c=.o)
PROGEXE := $(PROGSRC:.c=)
LIB :=  ../libosdinit.a ../../util/libosdutil.a

# auto-find by walking up from your path to mpicc
MPIDIR := $(dir $(shell type -p mpicc 2>/dev/null))
MPICC := $(shell type -p mpicc 2>/dev/null)
ifneq (,$(MPIDIR))
    MPIDIR := $(MPIDIR)..
    MPISRC := iospeed-mpi.c contention.c
    MPIOBJ := $(MPISRC:.c=.o)
    MPIEXE := $(MPISRC:.c=)
    # needed for some MPI builds
    PVFSDIR := $(dir $(shell type -p pvfs2-ls 2>/dev/null))..
    MPIINC := -I$(MPIDIR)/include
    MPILIB := -L/usr/local/mpich2-eth/lib -lmpich -L$(PVFSDIR)/lib -lpvfs2 -lrt
endif

include ../../Makedefs

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := $(OPT) -D_GNU_SOURCE
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I.. -I../..

# for blk-iospeed
CFLAGS += -Dsg_io_v4=sg_io_v4_v2622
#CFLAGS += -Dsg_io_v4=sg_io_v4_v2624

all:: $(LIB) $(PROGEXE) $(MPIEXE)

$(PROGEXE): %: %.o $(LIB)
	$(CC) -o $@ $^ $(LIB) -lm

$(MPIEXE): %: %.o $(LIB)
	$(MPICC) -o $@ $^ $(MPILIB) $(LIB) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(MPIOBJ): %.o: %.c
	$(MPICC) $(CFLAGS) $(MPIINC) -c $< -o $@

ifeq (,$(filter clean distclean dist,$(MAKECMDGOALS)))
-include .depend
endif
all:: .depend
.depend: $(PROGSRC) $(INC) Makefile
	@$(CC) $(CPP_M) $(CFLAGS) $(PROGSRC) > .depend

clean:
	rm -f $(PROGOBJ) $(PROGEXE) $(MPIOBJ) $(MPIEXE)
	rm -f .depend

tags: FORCE
	ctags $(PROGSRC) $(MPISRC) $(INC)

FORCE:;

