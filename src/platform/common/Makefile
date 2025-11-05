SRC:=$(wildcard *.c)
OBJ:=$(SRC:%.c=$(ODIR)/common/%.o)
BIN:=$(ODIR)/common/common.a

ODIR_STRUCTURE:=$(ODIR)/common

all: $(ODIR_STRUCTURE) $(BIN)

$(ODIR_STRUCTURE):
	mkdir -p $@

$(BIN): $(OBJ)
	ar rcs $@ $^ 

$(ODIR)/common/%.o: ./%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD -MP $(LDLIBS)

-include $(OBJ:.o=.d)
