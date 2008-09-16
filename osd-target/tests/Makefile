#
# OSD target test suite makefile
#
# $Id: $
#

SRC := $(wildcard *.c)
INC := $(wildcard *.h)
DEP := .depend
OBJ := $(SRC:.c=.o)
TESTS := $(wildcard *test.c)
EXE := $(TESTS:.c=)
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

$(EXE): %: %.o cdb-manip.o $(LIBOSD) 
	$(CC) -o $@ $^ -lsqlite3

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

#depend
ifneq (clean, $(MAKECMDGOALS))
-include $(DEP)
endif

all :: $(DEP)
$(DEP) :
	@$(CC) $(CPP_M) $(CFLAGS) $(SRC) > $(DEP) 
	
clean:
	rm -f $(EXE) $(OBJ) $(DEP)
