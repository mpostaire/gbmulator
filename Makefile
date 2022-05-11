SDIR=src
ODIR=out
IDIR=$(SDIR)
CFLAGS=-std=gnu11 -Wall -O2 -I$(IDIR)
LDLIBS=-lSDL2
CC=gcc
MAIN=gbmulator
EXEC=$(MAIN)

# exclude $(MAIN).c if 'make wasm' or 'make debug_wasm' is called, else exclude $(SDIR)/$(MAIN)_wasm.c
ifneq (,$(findstring wasm,$(MAKECMDGOALS)))
EXCLUDES:=$(SDIR)/$(MAIN).c
else ifneq (,$(findstring debug_wasm,$(MAKECMDGOALS)))
EXCLUDES:=$(SDIR)/$(MAIN).c
else
EXCLUDES:=$(SDIR)/gbmulator_wasm.c $(SDIR)/base64.c
endif

# recursive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SRC=$(filter-out $(EXCLUDES),$(call rwildcard,$(SDIR),*.c))
OBJ=$(SRC:$(SDIR)/%.c=$(ODIR)/%.o)

HEADERS=$(call rwildcard,$(IDIR),*.h)
HEADERS:=$(HEADERS:$(IDIR)/%=$(ODIR)/%)
# ODIR and its subdirectories structure to mkdir if they don't exist
ODIR_STRUCTURE:=$(sort $(foreach d,$(OBJ) $(HEADERS),$(subst /$(lastword $(subst /, ,$d)),,$d)))

all: $(ODIR_STRUCTURE) $(MAIN)

debug: CFLAGS+=-g -Og -DDEBUG
debug: all

wasm: CC:=emcc
wasm: LDLIBS:=
wasm: CFLAGS+=-O3
wasm: $(ODIR_STRUCTURE) index.html

debug_wasm: wasm
	emrun index.html

index.html: $(OBJ) template.html
	$(CC) -o $@ $(OBJ) $(CFLAGS) -sWASM=1 -sUSE_SDL=2 -sEXPORTED_RUNTIME_METHODS=[ccall] --shell-file template.html -lidbfs.js

$(MAIN): $(OBJ)
	$(CC) -o $(EXEC) $^ $(CFLAGS) $(LDLIBS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

$(ODIR_STRUCTURE):
	mkdir -p $@

run: all
	./$(EXEC)

check: $(SDIR)/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -rf $(ODIR) index.{html,js,wasm}

cleaner: clean
	rm -f $(EXEC)

install:
	install -m 0755 $(EXEC) /usr/bin

uninstall:
	rm -f /usr/bin/$(EXEC)

-include $(foreach d,$(ODIR_STRUCTURE),$d/*.d)

.PHONY: all clean run install uninstall debug wasm debug_wasm
