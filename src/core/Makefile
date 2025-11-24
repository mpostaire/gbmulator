SRC:=$(wildcard *.c) $(wildcard gb/*.c) $(wildcard gba/*.c) $(wildcard gbprinter/*.c)
OBJ:=$(SRC:%.c=$(ODIR)/core/%.o)
BIN:=$(ODIR)/core/core.a

ODIR_STRUCTURE:=$(ODIR)/core/{gb,gba,gbprinter}

all: $(ODIR_STRUCTURE) $(BIN)

$(ODIR_STRUCTURE):
	mkdir -p $@

$(BIN): $(OBJ)
	ar rcs $@ $^

$(ODIR)/core/%.o: ./%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD -MP $(LDLIBS)

-include $(OBJ:.o=.d)
