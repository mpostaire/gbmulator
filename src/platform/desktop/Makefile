SRC:=$(wildcard *.c) resources.c
OBJ:=$(SRC:%.c=$(ODIR)/desktop/%.o)
BIN:=$(ODIR)/gbmulator

ODIR_STRUCTURE:=$(ODIR)/desktop

UI:=$(wildcard ui/*.ui)

all: $(ODIR_STRUCTURE) $(BIN)

$(ODIR)/desktop/resources.c: ui/gbmulator.gresource.xml $(UI)
	glib-compile-resources $< --target=$@ --generate-source

$(ODIR_STRUCTURE):
	mkdir -p $@

$(BIN): $(OBJ)
	$(CC) -o $(BIN) $^ $(ODIR)/common/common.a $(ODIR)/core/core.a $(CFLAGS) $(LDLIBS)

$(ODIR)/desktop/%.o: ./%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD -MP $(LDLIBS)

-include $(OBJ:.o=.d)
