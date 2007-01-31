#
# OSD target makefile
#

SRC := attr.c db.c obj.c osd-schema.c osd.c util.c cdb.c
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
COPTS := -g
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I.. -Wno-unused

.SUFFIXES: .c .o .i

all: tgtd

tgtd: $(STGTLIB) $(OSDTARGETLIB) $(UTILLIB)
	$(LD) -o $@ $^ $(LIBS)

$(OSDTARGETLIB): $(OBJ)
	ar cr $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

osd-schema.c: osd.schema
	( echo "const char osd_schema[] =";\
	  sed 's/^/"/; s/$$/\\n"/' < $< ;\
	  echo ";" ) > $@

$(OBJ): $(INC)

clean:
	rm -f tgtd $(OSDTARGETLIB) $(OBJ) osd-schema.c

tags: FORCE
	ctags $(SRC) $(INC)

FORCE:;
