#
# OSD initiator tests
#

PROGSRC := sgio.c generic_iface_test.c iovec.c iospeed.c latency.c
PROGOBJ := $(PROGSRC:.c=.o)
PROGEXE := $(PROGSRC:.c=)
LIB :=  ../libosdinit.a ../../util/libosdutil.a

MPISRC := iospeed-mpi.c
MPIOBJ := $(MPISRC:.c=.o)
MPIEXE := $(MPISRC:.c=)
# auto-find by walking up from your path to mpicc
MPIDIR := /usr/local/mpich2-eth-pvfs-openib
# needed for some MPI builds
PVFSDIR := /home/dennis/OSD/osd/pvfs/install
MPIINC := -I$(MPIDIR)/include
MPILIB := -L$(MPIDIR)/lib -lmpich -L$(PVFSDIR)/lib -lpvfs2 -lrt

include ../../Makedefs

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := $(OPT)
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

