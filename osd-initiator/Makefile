#
# OSD initiator library
#

SRC := device.c sync.c drivelist.c command.c sense.c
INC := device.h sync.h drivelist.h command.h sense.h
OBJ := $(SRC:.c=.o)
LIB := libosdinit.a

include ../Makedefs

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := $(OPT) -fPIC
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes -Wstrict-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I..
CFLAGS += -Dsg_io_v4=sg_io_v4_v2622
#CFLAGS += -Dsg_io_v4=sg_io_v4_v2624

all:: $(LIB) $(OBJ)

$(LIB): $(OBJ)
	ar cr $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ifeq (,$(filter clean distclean dist,$(MAKECMDGOALS)))
-include .depend
endif
all:: .depend
.depend: $(SRC) $(INC) Makefile
	@$(CC) $(CPP_M) $(CFLAGS) $(SRC) > .depend

clean:
	rm -f $(LIB) $(OBJ)
	rm -f .depend

tags: FORCE
	ctags $(SRC) $(INC)

FORCE:;

# distribution tarball
.PHONY: dist
MV := osd-initiator-$(shell date +%Y%m%d)
dist:
	rm -rf $(MV)
	mkdir $(MV)
	svn list -R | rsync --files-from=/dev/stdin --exclude=.svn ./ $(MV)/
	tar cf - $(MV) | bzip2 -9c > $(MV).tar.bz2
	rm -rf $(MV)

