#
# OSD initiator tests
#

PROGSRC := sgio.c generic_iface_test.c iovec.c iospeed.c
PROGOBJ := $(PROGSRC:.c=.o)
PROGEXE := $(PROGSRC:.c=)
LIB := ../libosdinit.a ../../util/libosdutil.a

include ../../Makedefs

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := $(OPT)
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I.. -I../..

all:: $(LIB) $(PROGEXE)

%: %.o $(LIB)
	$(CC) -o $@ $^ $(LIB)

iospeed: %: %.o $(LIB)
	$(CC) -o $@ $^ $(LIB) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ifeq (,$(filter clean distclean dist,$(MAKECMDGOALS)))
-include .depend
endif
all:: .depend
.depend: $(PROGSRC) $(INC) Makefile
	@$(CC) $(CPP_M) $(CFLAGS) $(PROGSRC) > .depend

clean:
	rm -f $(PROGOBJ) $(PROGEXE)
	rm -f .depend

tags: FORCE
	ctags $(PROGSRC) $(INC)

FORCE:;

