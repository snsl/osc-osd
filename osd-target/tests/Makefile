#
# OSD target test suite makefile
#
# $Id: $
#

SRC := $(wildcard *.c)
INC := $(wildcard *.h)
OBJ := $(SRC:.c=.o)
EXE := $(SRC:.c=)
LIBOSD := ../libosd.a

CC := gcc
COPTS := -g
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I. -I.. 

.SUFFIXES: .c .o .i

# default target
all :: $(EXE)

$(EXE): %: %.o $(LIBOSD)
	$(CC) -o $@ $^ -lsqlite3

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBOSD) :
	make -f ../Makefile

clean:
	rm -f $(EXE) $(OBJ)
