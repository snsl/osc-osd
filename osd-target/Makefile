#
# OSD target makefile
#
# $Id: $
#

SRC := $(wildcard *.c)
INC := $(wildcard *.h)
OBJ := $(SRC:.c=.o)
LIB := libosd.a

CC := gcc
CPP_M := -MM
LD := $(CC)
COPTS := -g
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I. 

.SUFFIXES: .c .o .i

# default target
all :: $(LIB) $(OBJ)

$(LIB) : $(OBJ)
	ar cr $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIB) $(OBJ)
