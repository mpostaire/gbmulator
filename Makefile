SDIR=src
ODIR=out
IDIR=$(SDIR)
CFLAGS=-std=gnu11 -Wall -O2 -I$(IDIR)
LDLIBS=-lSDL2
CC=gcc
EXEC=gbmulator

# exclude $(SRC)/platform/desktop/* if 'make web' or 'make debug_web' is called, else exclude $(SRC)/platform/web/*
ifneq (,$(findstring web,$(MAKECMDGOALS)))
EXCLUDES:=$(wildcard $(SDIR)/platform/desktop/*)
else
EXCLUDES:=$(wildcard $(SDIR)/platform/web/*)
endif

# recursive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SRC=$(filter-out $(EXCLUDES),$(call rwildcard,$(SDIR),*.c))
OBJ=$(SRC:$(SDIR)/%.c=$(ODIR)/%.o)

HEADERS=$(call rwildcard,$(IDIR),*.h)
HEADERS:=$(HEADERS:$(IDIR)/%=$(ODIR)/%)
# ODIR and its subdirectories structure to mkdir if they don't exist
ODIR_STRUCTURE:=$(sort $(foreach d,$(OBJ) $(HEADERS),$(subst /$(lastword $(subst /, ,$d)),,$d)))

all: $(ODIR_STRUCTURE) $(EXEC)

debug: CFLAGS+=-g -O0
debug: all

web: CC:=emcc
web: LDLIBS:=
web: CFLAGS+=-O3 -sUSE_SDL=2
web: $(ODIR_STRUCTURE) docs docs/index.html

debug_web: web
	emrun docs/index.html

docs/index.html: $(SDIR)/platform/web/template.html $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -sINITIAL_MEMORY=32MB -sWASM=1 -sUSE_SDL=2 -sEXPORTED_RUNTIME_METHODS=[ccall] -sASYNCIFY --shell-file $< -lidbfs.js

$(EXEC): $(OBJ)
	$(CC) -o $(EXEC) $^ $(CFLAGS) $(LDLIBS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

$(ODIR_STRUCTURE):
	mkdir -p $@

docs:
	mkdir -p $@

run: all
	./$(EXEC) "roms/tests/cgb-acid2.gbc"

check: $(SDIR)/**/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -rf $(ODIR) index.{html,js,wasm}

cleaner: clean
	rm -f $(EXEC)

install:
	install -m 0755 $(EXEC) /usr/bin
	install -m 0644 $(SDIR)/platform/desktop/gbmulator.desktop /usr/share/applications

uninstall:
	rm -f /usr/bin/$(EXEC)
	rm -f /usr/share/applications/gbmulator.desktop

-include $(foreach d,$(ODIR_STRUCTURE),$d/*.d)

.PHONY: all clean run install uninstall debug web debug_web
