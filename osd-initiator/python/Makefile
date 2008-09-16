#
# Build python shared library from osd initiator sources.
#

# edit these files
SRC := pyosd.c attr.c command.c device.c drivelist.c const.c
OBJ := $(SRC:.c=.o)
LIB := pyosd.so

# external libraries being wrapped for python
HASPYCFG := $(shell hash python-config > /dev/null 2>&1 && echo yes)
ifeq ($(HASPYCFG),yes)
    INCDIR := $(shell python-config --includes)
else
    # mad guess
    INCDIR := -I/usr/include/python2.4
endif
INCDIR += -I../..
OSDLIBS := ../libosdinit.a ../../osd-util/libosdutil.a

-include ../../Makedefs

# usual definitions
CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := $(OPT) -fPIC
CWARN := -Wall -W -Wno-missing-field-initializers \
         -Wpointer-arith -Wcast-align -Wcast-qual \
	 -Wbad-function-cast -Wundef -Wmissing-prototypes -Wstrict-prototypes \
	 -Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) $(INCDIR)

all:: $(LIB) writefile.py

$(LIB): $(OBJ) $(OSDLIBS)
	gcc -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

writefile.py:
	ln -fs readfile.py writefile.py

ifeq (,$(filter clean distclean dist,$(MAKECMDGOALS)))
-include .depend
endif
all:: .depend
.depend: $(INC) Makefile
	$(CC) $(CPP_M) $(INCDIR) $(SRC) > .depend

clean:
	rm -f $(LIB) $(OBJ)
	rm -f .depend tags
	rm -f writefile.py

tags: FORCE
	ctags $(SRC)

FORCE:;

