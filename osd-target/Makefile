#
# OSD target makefile
#

SRC := attr.c db.c obj.c osd-schema.c osd.c cdb.c osd-sense.c list-entry.c
SRC += osd-schema.c
INC := attr.h db.h obj.h osd-types.h osd.h cdb.h target-defs.h list-entry.h \
       target-sense.h
DEP := .depend
OBJ := $(SRC:.c=.o)
TESTDIR := ./tests/
UTILDIR := ../util/
STGTDIR := ../stgt/
INITDIR := ../osd_initiator/
OSDTARGETLIB := libosdtgt.a

STGTLIB := ../stgt/libstgt.a
UTILLIB := ../util/libosdutil.a
LIBS := -lm -lcrypto -lsqlite3 -laio -lavahi-core -lavahi-common

include ../Makedefs

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := $(OPT) -fPIC -D_GNU_SOURCE
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs 
CFLAGS := $(COPTS) $(CWARN) -I.. -Wno-unused -I../util

.PHONY: UTIL TESTS STGT

.SUFFIXES: .c .o .i

all :: TESTS tgtd

tgtd: $(STGTLIB) $(OSDTARGETLIB) $(UTILLIB)
	$(LD) -o $@ $^ $(LIBS)

TESTS: $(UTILLIB) $(OSDTARGETLIB)
	make -C $(TESTDIR)

$(OSDTARGETLIB): $(OBJ)
	ar cr $@ $^

$(STGTLIB): STGT
	make -C $(STGTDIR)

$(UTILLIB): UTIL
	make -C $(UTILDIR)

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
