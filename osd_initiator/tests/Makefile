#
# OSD initiator tests
#

PROGSRC := basic_write.c
PROGOBJ := $(PROGSRC:.c=.o)
PROGEXE := $(PROGSRC:.c=)
LIB := ../libosd/libosd.a

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I.. -Wno-unused

all:: $(LIB) $(PROGEXE)

%: %.o
	$(CC) -o $@ $^ $(LIB)

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

