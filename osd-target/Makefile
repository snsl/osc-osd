#
# OSD target makefile
#

SRC := attr.c db.c obj.c osd-schema.c osd.c util.c cdb.c osd-sense.c
SRC += osd-schema.c
INC := attr.h db.h obj.h osd-types.h osd.h util.h osd-sense.h cdb.h osd-defs.h
OBJ := $(SRC:.c=.o)
OSDTARGETLIB := libosd.a

STGTLIB := ../stgt/libstgt.a
UTILLIB := ../util/libosdutil.a
LIBS := -lcrypto -lsqlite3 -laio

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g -D_GNU_SOURCE
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I.. -Wno-unused

.PHONY: UTIL TESTS

.SUFFIXES: .c .o .i

all: TESTS UTIL tgtd

tgtd: $(STGTLIB) $(OSDTARGETLIB) $(UTILLIB)
	$(LD) -o $@ $^ $(LIBS)

$(OSDTARGETLIB): $(OBJ)
	ar cr $@ $^

$(STGTLIB):
	make -C ../stgt

$(UTILLIB) UTIL:
	make -C ../util/

TESTS: UTIL $(OSDTARGETLIB)
	make -C ./tests/

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

osd-schema.c: osd.schema
	( echo "const char osd_schema[] =";\
	  sed 's/^/"/; s/$$/\\n"/' < $< ;\
	  echo ";" ) > $@

$(OBJ): $(INC)

clean:
	make -C ../util clean
	rm -f tgtd $(OSDTARGETLIB) $(OBJ) osd-schema.c
	make -C ./tests clean

tags: FORCE
	ctags $(SRC) $(INC)

FORCE:;
