SDIR=src
ODIR=out
IDIR=$(SDIR)
CFLAGS=-std=gnu11 -Wall -O2 -I$(IDIR)
LDLIBS=-lSDL2 $(shell pkg-config --libs zlib 2> /dev/null && echo -n "-D__HAS_ZLIB__")
CC=gcc
EXEC=gbmulator

# recursive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# exclude $(SDIR)/platform/{desktop,android}/* if 'make web' or 'make debug_web' is called, else exclude $(SDIR)/platform/{web,android}/*
ifneq (,$(findstring web,$(MAKECMDGOALS)))
EXCLUDES:=$(wildcard $(SDIR)/platform/desktop/*) $(call rwildcard,$(SDIR)/platform/android,*)
PLATFORM_ODIR=$(ODIR)/web
else
EXCLUDES:=$(wildcard $(SDIR)/platform/web/*) $(call rwildcard,$(SDIR)/platform/android,*)
PLATFORM_ODIR=$(ODIR)/desktop
endif

SRC=$(filter-out $(EXCLUDES),$(call rwildcard,$(SDIR),*.c))
OBJ=$(SRC:$(SDIR)/%.c=$(PLATFORM_ODIR)/%.o)

HEADERS=$(filter-out $(EXCLUDES),$(call rwildcard,$(IDIR),*.h))
HEADERS:=$(HEADERS:$(IDIR)/%=$(PLATFORM_ODIR)/%)
# PLATFORM_ODIR and its subdirectories' structure to mkdir if they don't exist
PLATFORM_ODIR_STRUCTURE:=$(sort $(foreach d,$(OBJ) $(HEADERS),$(subst /$(lastword $(subst /, ,$d)),,$d)))

ICONDIR=icons
ICONS= \
	$(ICONDIR)/192x192/$(EXEC).png \
	$(ICONDIR)/144x144/$(EXEC).png \
	$(ICONDIR)/128x128/$(EXEC).png \
	$(ICONDIR)/96x96/$(EXEC).png \
	$(ICONDIR)/72x72/$(EXEC).png \
	$(ICONDIR)/64x64/$(EXEC).png \
	$(ICONDIR)/48x48/$(EXEC).png \
	$(ICONDIR)/32x32/$(EXEC).png \
	$(ICONDIR)/16x16/$(EXEC).png

all: desktop

desktop: $(PLATFORM_ODIR_STRUCTURE) $(EXEC) $(ICONS)

debug: CFLAGS+=-g -O0
debug: all

profile: CFLAGS+=-p
profile: run
	gprof ./$(EXEC) gmon.out > prof_output

# TODO this should also make a gbmulator.apk file in this project root dir (next do the gbmulator desktop binary) 
android: $(ICONS)
	cd $(SDIR)/platform/android/android-project && ./gradlew assemble

debug_android: android
	cd $(SDIR)/platform/android/android-project && ./gradlew installDebug
	adb shell am start -n io.github.mpostaire.gbmulator/.MainMenu

web: CC:=emcc
web: LDLIBS:=
web: CFLAGS+=-O3 -sUSE_SDL=2 -sUSE_ZLIB=1
web: $(PLATFORM_ODIR_STRUCTURE) $(ICONS) docs docs/index.html 

debug_web: web
	emrun docs/index.html

docs/index.html: $(SDIR)/platform/web/template.html $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -sABORTING_MALLOC=0 -sINITIAL_MEMORY=32MB -sWASM=1 -sUSE_SDL=2 -sEXPORTED_RUNTIME_METHODS=[ccall] -sASYNCIFY --shell-file $< -lidbfs.js

$(EXEC): $(OBJ)
	$(CC) -o $(EXEC) $^ $(CFLAGS) $(LDLIBS)

$(PLATFORM_ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

$(PLATFORM_ODIR_STRUCTURE):
	mkdir -p $@

docs:
	mkdir -p $@

$(ICONS): $(ICONDIR)/$(EXEC).svg
	mkdir -p $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@)
	convert -background none -size $(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@) $^ $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@)/$(EXEC).png
	[ $(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@) = 16x16 ] && cp $(ICONDIR)/16x16/$(EXEC).png docs/favicon.png || true
	[ $(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@) = 48x48 ] && cp $(ICONDIR)/48x48/$(EXEC).png src/platform/android/android-project/app/src/main/res/mipmap-mdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@) = 72x72 ] && cp $(ICONDIR)/72x72/$(EXEC).png src/platform/android/android-project/app/src/main/res/mipmap-hdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@) = 96x96 ] && cp $(ICONDIR)/96x96/$(EXEC).png src/platform/android/android-project/app/src/main/res/mipmap-xhdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@) = 144x144 ] && cp $(ICONDIR)/144x144/$(EXEC).png src/platform/android/android-project/app/src/main/res/mipmap-xxhdpi/ic_launcher.png || true
	[ $(patsubst $(ICONDIR)/%/$(EXEC).png,%,$@) = 192x192 ] && cp $(ICONDIR)/192x192/$(EXEC).png src/platform/android/android-project/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png || true

run: desktop
	./$(EXEC) "roms/pokemon_gold.gbc"

check: $(SDIR)/**/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -rf $(ODIR)

cleaner: clean
	rm -f $(EXEC)
	cd $(SDIR)/platform/android/android-project && ./gradlew clean

install:
	install -m 0755 $(EXEC) /usr/bin
	install -m 0644 $(SDIR)/platform/desktop/$(EXEC).desktop /usr/share/applications
	install -m 0644 $(ICONDIR)/$(EXEC).svg /usr/share/icons/hicolor/scalable/apps
	install -m 0644 $(ICONDIR)/192x192/$(EXEC).png /usr/share/icons/hicolor/192x192/apps
	install -m 0644 $(ICONDIR)/128x128/$(EXEC).png /usr/share/icons/hicolor/128x128/apps
	install -m 0644 $(ICONDIR)/96x96/$(EXEC).png /usr/share/icons/hicolor/96x96/apps
	install -m 0644 $(ICONDIR)/72x72/$(EXEC).png /usr/share/icons/hicolor/72x72/apps
	install -m 0644 $(ICONDIR)/64x64/$(EXEC).png /usr/share/icons/hicolor/64x64/apps
	install -m 0644 $(ICONDIR)/48x48/$(EXEC).png /usr/share/icons/hicolor/48x48/apps
	install -m 0644 $(ICONDIR)/32x32/$(EXEC).png /usr/share/icons/hicolor/32x32/apps
	install -m 0644 $(ICONDIR)/16x16/$(EXEC).png /usr/share/icons/hicolor/16x16/apps
	gtk-update-icon-cache
	update-desktop-database

uninstall:
	rm -f /usr/bin/$(EXEC)
	rm -f /usr/share/applications/$(EXEC).desktop
	rm -f /usr/share/icons/hicolor/scalable/apps/$(EXEC).svg
	rm -f /usr/share/icons/hicolor/{192x192,128x128,96x96,72x72,64x64,48x48,32x32,16x16}/apps/$(EXEC).png
	rm -f /usr/share/mime/application/$(EXEC).xml
	gtk-update-icon-cache
	update-desktop-database

-include $(foreach d,$(PLATFORM_ODIR_STRUCTURE),$d/*.d)

.PHONY: all clean run install uninstall debug web debug_web android debug_android
