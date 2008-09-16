#
# OSD initiator tests
#

PROGSRC := sgio.c generic_iface_test.c iovec.c iospeed.c latency.c 
PROGSRC += blk-iospeed.c
PROGOBJ := $(PROGSRC:.c=.o)
PROGEXE := $(PROGSRC:.c=)
LIB :=  ../libosdinit.a ../../util/libosdutil.a

MPISRC := iospeed-mpi.c
MPIOBJ := $(MPISRC:.c=.o)
MPIEXE := $(MPISRC:.c=)
# auto-find by walking up from your path to mpicc
MPIDIR := $(dir $(shell type -p mpicc 2>/dev/null))..
# needed for some MPI builds
PVFSDIR := $(dir $(shell type -p pvfs2-ls 2>/dev/null))..
MPIINC := -I$(MPIDIR)/include
MPILIB := -L$(MPIDIR)/lib -lmpich -L$(PVFSDIR)/lib -lpvfs2 -lrt

include ../../Makedefs

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := $(OPT) -D_GNU_SOURCE
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I.. -I../..

all:: $(LIB) $(PROGEXE) $(MPIEXE)

$(PROGEXE): %: %.o $(LIB)
	$(CC) -o $@ $^ $(LIB) -lm

$(MPIEXE): %: %.o $(LIB)
	$(CC) -o $@ $^ $(LIB) -lm $(MPILIB)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(MPIOBJ): %.o: %.c
	$(CC) $(CFLAGS) $(MPIINC) -c $< -o $@

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

