#
# OSD initiator library
#

SRC := device.c sync.c drivelist.c command.c sense.c
INC := device.h sync.h drivelist.h command.h sense.h osd_cmds.h
OBJ := $(SRC:.c=.o)
LIB := libosd.a

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g -fPIC
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes -Wstrict-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I..
CFLAGS += -I/usr/src/kernel/linux/include
#CFLAGS += -I/usr/src/kernel/pw/include

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

