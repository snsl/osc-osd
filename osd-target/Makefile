#
# OSD target makefile
#
# $Id: $
#

SRC := attr.c db.c obj.c osd-schema.c osd.c util.c
SRC += osd-schema.c
INC := attr.h db.h obj.h osd-types.h osd.h util.h
OBJ := $(SRC:.c=.o)
LIB := libosd.a

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I. -Wno-unused

.SUFFIXES: .c .o .i

# default target
all :: $(LIB) $(OBJ)

$(LIB) : $(OBJ)
	ar cr $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

osd-schema.c: osd.schema
	( echo "const char osd_schema[] =";\
	  sed 's/^/"/; s/$$/\\n"/' < $< ;\
	  echo ";" ) > $@

$(OBJ): $(INC)

clean:
	rm -f $(LIB) $(OBJ) osd-schema.c

tags: FORCE
	ctags $(SRC) $(INC)

FORCE:;
