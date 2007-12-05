#
# OSD target test suite makefile
#
# $Id: $
#

DEP := .depend
TESTS := $(wildcard *.c)
OBJ := $(TESTS:.c=.o)
EXE := $(TESTS:.c=)

CMD := command.c
CMD_OBJ := $(CMD:.c=.o)
CMD_SRC := ../../osd-initiator/$(CMD)
CMD_INC := $(CMD_SRC:.c=.h)

TMG_DIR := ../../benchmarks/osd-target/
TMG_TEST := create.c setattr.c getattr.c list.c query.c
TMG_TEST += set_member_attributes.c
TMG_SRC := $(addprefix $(TMG_DIR), $(TMG_TEST))
TMG_OBJ := $(TMG_TEST:.c=.o)
TMG_EXE := $(TMG_TEST:.c=)

LIBOSD := ../libosdtgt.a ../../osd-util/libosdutil.a 

-include ../../Makedefs
OPT := $(filter-out -DNDEBUG,$(OPT)) # don't turn off asserts!

CC := gcc
CPP_M := -MM
COPTS := $(OPT)
CWARN := -Wall -W -Wpointer-arith -Wwrite-strings -Wcast-align -Wcast-qual \
	-Wbad-function-cast -Wundef -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs
CFLAGS := $(COPTS) $(CWARN) -I. -I.. -I../../

.SUFFIXES: .c .o .i

# default target
all :: $(EXE) $(TMG_EXE)

$(EXE): %: %.o $(CMD_OBJ) $(LIBOSD) 
	$(CC) -o $@ $^ -lsqlite3 -lm 

$(TMG_EXE): %: %.o $(CMD_OBJ) $(LIBOSD) 
	$(CC) -o $@ $^ -lsqlite3 -lm

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(CMD_OBJ): $(CMD_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(TMG_OBJ): %.o: $(addprefix $(TMG_DIR), %.c)
	$(CC) $(CFLAGS) -c $< -o $@

#depend
ifneq (clean, $(MAKECMDGOALS))
-include $(DEP)
endif

all :: $(DEP)
$(DEP) :
	@$(CC) $(CPP_M) $(CFLAGS) $(TESTS) $(CMD_SRC) $(TMG_SRC) > $(DEP) 
	
clean:
	rm -f $(EXE) $(OBJ) $(CMD_OBJ) $(TMG_OBJ) $(TMG_EXE) $(DEP) 
