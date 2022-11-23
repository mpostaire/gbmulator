SDIR=src
ODIR=out
IDIR=$(SDIR)
CFLAGS=-std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type -O3 -I$(IDIR)
LDLIBS=$(shell pkg-config --libs zlib 2> /dev/null && echo -n "-D__HAVE_ZLIB__")
CC=gcc
BIN=gbmulator

# recursive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# exclude $(SDIR)/platform/{desktop,android}/* if 'make web' or 'make debug_web' is called, else exclude $(SDIR)/platform/{web,android}/*, etc.
ifneq (,$(findstring web,$(MAKECMDGOALS)))
EXCLUDES:=$(call rwildcard,$(SDIR)/platform/desktop,*) $(wildcard $(SDIR)/platform/desktop_sdl/*) $(call rwildcard,$(SDIR)/platform/android,*)
PLATFORM_ODIR=$(ODIR)/web
else ifneq (,$(findstring desktop_sdl,$(MAKECMDGOALS)))
EXCLUDES:=$(wildcard $(SDIR)/platform/desktop/*) $(wildcard $(SDIR)/platform/web/*) $(call rwildcard,$(SDIR)/platform/android,*)
PLATFORM_ODIR=$(ODIR)/desktop_sdl
else
EXCLUDES:=$(wildcard $(SDIR)/platform/web/*) $(call rwildcard,$(SDIR)/platform/android,*) $(call rwildcard,$(SDIR)/platform/desktop_sdl,*)
PLATFORM_ODIR=$(ODIR)/desktop
OBJ=$(PLATFORM_ODIR)/platform/desktop/resources.o
endif

SRC=$(filter-out $(EXCLUDES),$(call rwildcard,$(SDIR),*.c))
OBJ+=$(SRC:$(SDIR)/%.c=$(PLATFORM_ODIR)/%.o)

HEADERS=$(filter-out $(EXCLUDES),$(call rwildcard,$(IDIR),*.h))
HEADERS:=$(HEADERS:$(IDIR)/%=$(PLATFORM_ODIR)/%)
# PLATFORM_ODIR and its subdirectories' structure to mkdir if they don't exist
PLATFORM_ODIR_STRUCTURE:=$(sort $(foreach d,$(OBJ) $(HEADERS),$(subst /$(lastword $(subst /, ,$d)),,$d)))

ICONDIR=icons
ICONS= \
	$(ICONDIR)/192x192/$(BIN).png \
	$(ICONDIR)/144x144/$(BIN).png \
	$(ICONDIR)/128x128/$(BIN).png \
	$(ICONDIR)/96x96/$(BIN).png \
	$(ICONDIR)/72x72/$(BIN).png \
	$(ICONDIR)/64x64/$(BIN).png \
	$(ICONDIR)/48x48/$(BIN).png \
	$(ICONDIR)/32x32/$(BIN).png \
	$(ICONDIR)/16x16/$(BIN).png

UI:=$(wildcard $(SDIR)/platform/desktop/ui/*.ui)
SHADERS:=$(wildcard $(SDIR)/platform/desktop/ui/*.glsl)

all: desktop

debug: CFLAGS+=-g -O0
debug: all

desktop: CFLAGS+=$(shell pkg-config --cflags gtk4 libadwaita-1 manette-0.2 opengl glew openal x11)
desktop: LDLIBS+=$(shell pkg-config --libs gtk4 libadwaita-1 manette-0.2 opengl glew openal x11)
desktop: $(PLATFORM_ODIR_STRUCTURE) $(BIN) $(ICONS)

$(SDIR)/platform/desktop/resources.c: $(SDIR)/platform/desktop/ui/gbmulator.gresource.xml $(UI) $(SHADERS)
	glib-compile-resources $< --target=$@ --generate-source

desktop_sdl: CFLAGS+=$(shell pkg-config --cflags sdl2)
desktop_sdl: LDLIBS+=$(shell pkg-config --libs sdl2)
desktop_sdl: $(PLATFORM_ODIR_STRUCTURE) $(BIN) $(ICONS)

profile: CFLAGS+=-p
profile: run
	gprof ./$(BIN) gmon.out > prof_output

# TODO this should also make a gbmulator.apk file in this project root dir (next do the gbmulator desktop binary) 
android: $(ICONS)
	cd $(SDIR)/platform/android/android-project && ./gradlew assemble

debug_android: android
	cd $(SDIR)/platform/android/android-project && ./gradlew installDebug
	adb shell am start -n io.github.mpostaire.gbmulator/.MainMenu

web: CC:=emcc
web: LDLIBS:=
web: CFLAGS+=-sUSE_SDL=2 -sUSE_ZLIB=1
web: $(PLATFORM_ODIR_STRUCTURE) $(ICONS) docs docs/index.html 

debug_web: web
	emrun docs/index.html

docs/index.html: $(SDIR)/platform/web/template.html $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -sABORTING_MALLOC=0 -sINITIAL_MEMORY=32MB -sWASM=1 -sEXPORTED_RUNTIME_METHODS=[ccall] -sASYNCIFY --shell-file $< -lidbfs.js

$(BIN): $(OBJ)
	$(CC) -o $(BIN) $^ $(CFLAGS) $(LDLIBS)

$(PLATFORM_ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD $(LDLIBS)

$(PLATFORM_ODIR_STRUCTURE):
	mkdir -p $@

docs:
	mkdir -p $@

$(ICONS): $(ICONDIR)/$(BIN).svg
	mkdir -p $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BIN).png,%,$@)
	convert -background none -size $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) $^ $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BIN).png,%,$@)/$(BIN).png
	[ $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) = 16x16 ] && cp $(ICONDIR)/16x16/$(BIN).png docs/favicon.png || true
	[ $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) = 48x48 ] && cp $(ICONDIR)/48x48/$(BIN).png src/platform/android/android-project/app/src/main/res/mipmap-mdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) = 72x72 ] && cp $(ICONDIR)/72x72/$(BIN).png src/platform/android/android-project/app/src/main/res/mipmap-hdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) = 96x96 ] && cp $(ICONDIR)/96x96/$(BIN).png src/platform/android/android-project/app/src/main/res/mipmap-xhdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) = 144x144 ] && cp $(ICONDIR)/144x144/$(BIN).png src/platform/android/android-project/app/src/main/res/mipmap-xxhdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) = 192x192 ] && cp $(ICONDIR)/192x192/$(BIN).png src/platform/android/android-project/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png || true

run: desktop
	LIBGL_DRI3_DISABLE=1 ./$(BIN) "roms/tetris.gb"

check: $(SDIR)
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -rf $(ODIR)

cleaner: clean
	rm -f $(BIN) $(SDIR)/platform/desktop/resources.c
	cd $(SDIR)/platform/android/android-project && ./gradlew clean

install:
	install -m 0755 $(BIN) /usr/bin
	install -m 0644 $(SDIR)/platform/desktop/$(BIN).xml /usr/share/mime/application
	install -m 0644 $(SDIR)/platform/desktop/$(BIN).desktop /usr/share/applications
	install -m 0644 $(ICONDIR)/$(BIN).svg /usr/share/icons/hicolor/scalable/apps
	install -m 0644 $(ICONDIR)/192x192/$(BIN).png /usr/share/icons/hicolor/192x192/apps
	install -m 0644 $(ICONDIR)/128x128/$(BIN).png /usr/share/icons/hicolor/128x128/apps
	install -m 0644 $(ICONDIR)/96x96/$(BIN).png /usr/share/icons/hicolor/96x96/apps
	install -m 0644 $(ICONDIR)/72x72/$(BIN).png /usr/share/icons/hicolor/72x72/apps
	install -m 0644 $(ICONDIR)/64x64/$(BIN).png /usr/share/icons/hicolor/64x64/apps
	install -m 0644 $(ICONDIR)/48x48/$(BIN).png /usr/share/icons/hicolor/48x48/apps
	install -m 0644 $(ICONDIR)/32x32/$(BIN).png /usr/share/icons/hicolor/32x32/apps
	install -m 0644 $(ICONDIR)/16x16/$(BIN).png /usr/share/icons/hicolor/16x16/apps
	gtk-update-icon-cache
	update-desktop-database

uninstall:
	rm -f /usr/bin/$(BIN)
	rm -f /usr/share/applications/$(BIN).desktop
	rm -f /usr/share/icons/hicolor/scalable/apps/$(BIN).svg
	rm -f /usr/share/icons/hicolor/{192x192,128x128,96x96,72x72,64x64,48x48,32x32,16x16}/apps/$(BIN).png
	rm -f /usr/share/mime/application/$(BIN).xml
	gtk-update-icon-cache
	update-desktop-database

-include $(foreach d,$(PLATFORM_ODIR_STRUCTURE),$d/*.d)

.PHONY: all clean run install uninstall debug web debug_web android debug_android
