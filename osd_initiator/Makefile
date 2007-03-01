#
# OSD initiator library
#

SRC := kernel_interface.c user_interface_sgio.c diskinfo.c cdb_manip.c sense.c \
	pvfs_iface.c
INC := kernel_interface.h user_interface_sgio.h diskinfo.h cdb_manip.h sense.h \
	pvfs_iface.h
OBJ := $(SRC:.c=.o)
LIB := libosd.a

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g -fPIC
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes -Wstrict-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I.. -Wno-unused
CFLAGS += -I/usr/src/kernel/linux/include
#CFLAGS += -I/usr/src/kernel/pw/include -DTESTING_IOVEC

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

