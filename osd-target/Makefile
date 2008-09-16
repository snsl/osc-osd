#
# OSD target makefile
#

SRC := attr.c db.c obj.c osd-schema.c osd.c cdb.c osd-sense.c list-entry.c
SRC += osd-schema.c
INC := attr.h db.h obj.h osd-types.h osd.h osd-sense.h cdb.h osd-defs.h
INC += list-entry.h
DEP := .depend
OBJ := $(SRC:.c=.o)
TESTDIR := ./tests/
UTILDIR := ../util/
STGTDIR := ../stgt/
INITDIR := ../osd_initiator/
OSDTARGETLIB := libosd.a

STGTLIB := ../stgt/libstgt.a
UTILLIB := ../util/libosdutil.a
LIBS := -lcrypto -lsqlite3 -laio -lavahi-core -lavahi-common

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g -fPIC -D_GNU_SOURCE
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs 
CFLAGS := $(COPTS) $(CWARN) -I.. -Wno-unused

.PHONY: UTIL TESTS

.SUFFIXES: .c .o .i

all :: TESTS UTIL tgtd

tgtd: $(STGTLIB) $(OSDTARGETLIB) $(UTILLIB)
	$(LD) -o $@ $^ $(LIBS)

$(OSDTARGETLIB): $(OBJ)
	ar cr $@ $^

$(STGTLIB):
	make -C $(STGTDIR)

$(UTILLIB) UTIL:
	make -C $(UTILDIR)

TESTS: UTIL $(OSDTARGETLIB)
	make -C $(TESTDIR)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

osd-schema.c: osd.schema
	( echo "const char osd_schema[] =";\
	  sed 's/^/"/; s/$$/\\n"/' < $< ;\
	  echo ";" ) > $@

#depend
ifneq (clean, $(MAKECMDGOALS))
-include $(DEP)
endif

all :: $(DEP)
$(DEP) : osd-schema.c
	@$(CC) $(CPP_M) $(CFLAGS) $(SRC) > $(DEP) 

clean:
	make -C $(UTILDIR) clean
	rm -f tgtd $(OSDTARGETLIB) $(OBJ) $(DEP) osd-schema.c
	make -C $(TESTDIR) clean

tags: FORCE osd-schema.c
	ctags -R $(SRC) $(INC) $(TESTDIR) $(UTILDIR) $(INITDIR) osd-schema.c

FORCE:;
