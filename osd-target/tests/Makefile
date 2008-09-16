#
# OSD target test suite makefile
#
# $Id: $
#

DEP := .depend
TESTS := $(wildcard *test.c)
OBJ := $(TESTS:.c=.o)
EXE := $(TESTS:.c=)

CMD := command.c
CMD_OBJ := $(CMD:.c=.o)
CMD_SRC := ../../osd_initiator/$(CMD)
CMD_INC := $(CMD_SRC:.c=.h)

LIBOSD := ../libosd.a ../../util/libosdutil.a

CC := gcc
CPP_M := -MM
COPTS := -g
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I. -I.. -I../../

.SUFFIXES: .c .o .i

# default target
all :: $(EXE)

$(EXE): %: %.o $(CMD_OBJ) $(LIBOSD) 
	$(CC) -o $@ $^ -lsqlite3

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(CMD_OBJ): $(CMD_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

#depend
ifneq (clean, $(MAKECMDGOALS))
-include $(DEP)
endif

all :: $(DEP)
$(DEP) :
	@$(CC) $(CPP_M) $(CFLAGS) $(SRC) $(CMD_SRC) > $(DEP) 
	
clean:
	rm -f $(EXE) $(OBJ) $(CMD_OBJ) $(DEP)
