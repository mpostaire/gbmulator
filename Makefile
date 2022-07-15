SDIR=src
ODIR=out
IDIR=$(SDIR)
CFLAGS=-std=gnu11 -Wall -O2 -I$(IDIR)
LDLIBS=-lSDL2
CC=gcc
EXEC=gbmulator

# exclude $(SDIR)/platform/desktop/* if 'make web' or 'make debug_web' is called, else exclude $(SDIR)/platform/web/*
ifneq (,$(findstring web,$(MAKECMDGOALS)))
EXCLUDES:=$(wildcard $(SDIR)/platform/desktop/*)
PLATFORM_ODIR=$(ODIR)/web
else
EXCLUDES:=$(wildcard $(SDIR)/platform/web/*)
PLATFORM_ODIR=$(ODIR)/desktop
endif

# recursive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SRC=$(filter-out $(EXCLUDES),$(call rwildcard,$(SDIR),*.c))
OBJ=$(SRC:$(SDIR)/%.c=$(PLATFORM_ODIR)/%.o)

HEADERS=$(call rwildcard,$(IDIR),*.h)
HEADERS:=$(HEADERS:$(IDIR)/%=$(PLATFORM_ODIR)/%)
# PLATFORM_ODIR and its subdirectories' structure to mkdir if they don't exist
PLATFORM_ODIR_STRUCTURE:=$(sort $(foreach d,$(OBJ) $(HEADERS),$(subst /$(lastword $(subst /, ,$d)),,$d)))

ICONDIR=icons
ICONS=$(ICONDIR)/128x128/${EXEC}.png $(ICONDIR)/64x64/${EXEC}.png $(ICONDIR)/48x48/${EXEC}.png $(ICONDIR)/32x32/${EXEC}.png $(ICONDIR)/16x16/${EXEC}.png

all: desktop

desktop: $(PLATFORM_ODIR_STRUCTURE) $(EXEC) $(ICONS)

debug: CFLAGS+=-g -O0
debug: all

web: CC:=emcc
web: LDLIBS:=
web: CFLAGS+=-O3 -sUSE_SDL=2
web: $(PLATFORM_ODIR_STRUCTURE) docs docs/index.html $(ICONS)

debug_web: web
	emrun docs/index.html

docs/index.html: $(SDIR)/platform/web/template.html $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -sINITIAL_MEMORY=32MB -sWASM=1 -sUSE_SDL=2 -sEXPORTED_RUNTIME_METHODS=[ccall] -sASYNCIFY --shell-file $< -lidbfs.js

$(EXEC): $(OBJ)
	$(CC) -o $(EXEC) $^ $(CFLAGS) $(LDLIBS)

$(PLATFORM_ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

$(PLATFORM_ODIR_STRUCTURE):
	mkdir -p $@

docs:
	mkdir -p $@

$(ICONS): $(ICONDIR)/${EXEC}.svg
	mkdir -p $(ICONDIR)/$(patsubst $(ICONDIR)/%/${EXEC}.png,%,$@)
	convert -background none -resize $(patsubst $(ICONDIR)/%/${EXEC}.png,%,$@) $^ $(ICONDIR)/$(patsubst $(ICONDIR)/%/${EXEC}.png,%,$@)/${EXEC}.png
	[ $(patsubst $(ICONDIR)/%/${EXEC}.png,%,$@) = 16x16 ] && cp $(ICONDIR)/16x16/${EXEC}.png docs/favicon.png || true

run: desktop
	./$(EXEC) "roms/tests/cgb-acid2.gbc"

check: $(SDIR)/**/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -rf $(ODIR)

cleaner: clean
	rm -f $(EXEC)

install:
	install -m 0755 $(EXEC) /usr/bin
	install -m 0644 $(SDIR)/platform/desktop/$(EXEC).desktop /usr/share/applications
	install -m 0644 $(ICONDIR)/${EXEC}.svg /usr/share/icons/hicolor/scalable/apps
	install -m 0644 $(ICONDIR)/128x128/${EXEC}.png /usr/share/icons/hicolor/128x128/apps
	install -m 0644 $(ICONDIR)/64x64/${EXEC}.png /usr/share/icons/hicolor/64x64/apps
	install -m 0644 $(ICONDIR)/48x48/${EXEC}.png /usr/share/icons/hicolor/48x48/apps
	install -m 0644 $(ICONDIR)/32x32/${EXEC}.png /usr/share/icons/hicolor/32x32/apps
	install -m 0644 $(ICONDIR)/16x16/${EXEC}.png /usr/share/icons/hicolor/16x16/apps
	gtk-update-icon-cache
	update-desktop-database

uninstall:
	rm -f /usr/bin/$(EXEC)
	rm -f /usr/share/applications/$(EXEC).desktop
	rm -f /usr/share/icons/hicolor/scalable/apps/$(EXEC).svg
	rm -f /usr/share/icons/hicolor/{128x128,64x64,48x48,32x32,16x16}/apps/$(EXEC).png
	gtk-update-icon-cache
	update-desktop-database

-include $(foreach d,$(PLATFORM_ODIR_STRUCTURE),$d/*.d)

.PHONY: all clean run install uninstall debug web debug_web
