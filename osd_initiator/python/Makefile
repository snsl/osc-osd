#
# Build python shared library from osd initiator sources.
#

# edit these files
SRC := pyosd.c attr.c command.c device.c drivelist.c const.c
OBJ := $(SRC:.c=.o)
LIB := pyosd.so

# external libraries being wrapped for python
INCDIR := -I/usr/include/python2.4 -I../..
OSDLIBS := ../libosdinit.a ../../util/libosdutil.a

# usual definitions
CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g -fPIC
CWARN := -Wall -W -Wno-missing-field-initializers \
         -Wpointer-arith -Wcast-align -Wcast-qual \
	 -Wbad-function-cast -Wundef -Wmissing-prototypes -Wstrict-prototypes \
	 -Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) $(INCDIR)

all:: $(LIB)

$(LIB): $(OBJ) $(OSDLIBS)
	gcc -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ifeq (,$(filter clean distclean dist,$(MAKECMDGOALS)))
-include .depend
endif
all:: .depend
.depend: $(INC) Makefile
	$(CC) $(CPP_M) $(INCDIR) $(SRC) > .depend

clean:
	rm -f $(LIB) $(OBJ)
	rm -f .depend tags

tags: FORCE
	ctags $(SRC)

FORCE:;

