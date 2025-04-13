SDIR=src
ODIR=build
IDIR=$(SDIR)
CFLAGS=-std=gnu23 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type -O3 -I$(IDIR) -DVERSION=$(shell git rev-parse --short HEAD)
LDLIBS=
CC=gcc
BIN=gbmulator

# recursive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# exclude $(SDIR)/platform/{desktop,android}/* if 'make web' or 'make debug_web' is called, else exclude $(SDIR)/platform/{web,android}/*, etc.
ifneq (,$(findstring web,$(MAKECMDGOALS)))
EXCLUDES:=$(call rwildcard,$(SDIR)/platform/desktop,*) $(call rwildcard,$(SDIR)/platform/android,*)
PLATFORM_ODIR=$(ODIR)/web
else
EXCLUDES:=$(wildcard $(SDIR)/platform/web/*) $(call rwildcard,$(SDIR)/platform/android,*)
PLATFORM_ODIR=$(ODIR)/desktop
OBJ=$(PLATFORM_ODIR)/platform/desktop/resources.o
endif

EXCLUDES+=$(call rwildcard,$(SDIR)/bootroms,*)

SRC=$(filter-out $(EXCLUDES),$(call rwildcard,$(SDIR),*.c))
OBJ+=$(SRC:$(SDIR)/%.c=$(PLATFORM_ODIR)/%.o)

HEADERS=$(filter-out $(EXCLUDES),$(call rwildcard,$(IDIR),*.h))
HEADERS:=$(HEADERS:$(IDIR)/%=$(PLATFORM_ODIR)/%)
# PLATFORM_ODIR and its subdirectories' structure to mkdir if they don't exist
PLATFORM_ODIR_STRUCTURE:=$(sort $(foreach d,$(OBJ) $(HEADERS),$(subst /$(lastword $(subst /, ,$d)),,$d)))

ICONDIR=images/icons
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

debug: CFLAGS+=-ggdb -O0
# debug: CFLAGS+=-DDEBUG
debug: all

desktop: CFLAGS+=$(shell pkg-config --cflags gtk4 libadwaita-1 zlib manette-0.2 opengl openal gstreamer-1.0) -fanalyzer
desktop: LDLIBS+=$(shell pkg-config --libs gtk4 libadwaita-1 zlib manette-0.2 opengl openal gstreamer-1.0)
desktop: $(PLATFORM_ODIR_STRUCTURE) $(BIN) $(ICONS)

$(SDIR)/platform/desktop/resources.c: $(SDIR)/platform/desktop/ui/gbmulator.gresource.xml $(UI) $(SHADERS)
	glib-compile-resources $< --target=$@ --generate-source

profile: CFLAGS+=-pg
profile: run
	gprof ./$(BIN) gmon.out > prof_output

# TODO this should also make a gbmulator.apk file in this project root dir (next do the gbmulator desktop binary) 
android: $(SDIR)/core/gb/boot.c
	cd $(SDIR)/platform/android/android-project && ./gradlew build

debug_android: android
	cd $(SDIR)/platform/android/android-project && ./gradlew installDebug
	adb shell am start -n io.github.mpostaire.gbmulator/.MainMenu

web_build:
	mkdir -p web_build

web_build/favicon.png: $(SDIR)/platform/web/favicon.png
	cp $^ $@
web_build/style.css: $(SDIR)/platform/web/style.css
	cp $^ $@

web: CC:=emcc
web: LDLIBS:=
web: CFLAGS+=-sUSE_ZLIB=1
web: $(PLATFORM_ODIR_STRUCTURE) web_build web_build/favicon.png web_build/style.css web_build/index.html

debug_web: web
	emrun web_build/index.html

web_build/index.html: $(SDIR)/platform/web/template.html $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -lopenal -sINITIAL_MEMORY=96MB -sUSE_WEBGL2=1 -sWASM=1 -sEXPORTED_FUNCTIONS="['_main', '_malloc']" -sEXPORTED_RUNTIME_METHODS="['ccall']" -sASYNCIFY --shell-file $<

test:
	$(MAKE) -C test

$(BIN): $(OBJ)
	$(CC) -o $(BIN) $^ $(CFLAGS) $(LDLIBS)

$(PLATFORM_ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD -MP $(LDLIBS)

$(PLATFORM_ODIR_STRUCTURE):
	mkdir -p $@

# Build boot roms (taken and modified from SameBoy emulator)
$(SDIR)/core/gb/boot.c: $(ODIR)/bootroms/gb/dmg_boot $(ODIR)/bootroms/gb/cgb_boot
	cd $(ODIR)/bootroms/gb && xxd -i dmg_boot > ../../../$(SDIR)/core/gb/boot.c && xxd -i cgb_boot >> ../../../$(SDIR)/core/gb/boot.c

$(ODIR)/bootroms/gb/gbmulator_logo.1bpp: $(SDIR)/bootroms/gb/gbmulator_logo.png
	-@mkdir -p $(dir $@)
	rgbgfx -d 1 -Z -o $@ $<

$(ODIR)/bootroms/gb/gbmulator_logo.pb8: $(ODIR)/bootroms/gb/gbmulator_logo.1bpp $(ODIR)/bootroms/gb/pb8
	$(ODIR)/bootroms/gb/pb8 -l 384 $< $@

# force gcc here to avoid compiling with emcc
$(ODIR)/bootroms/gb/pb8: $(SDIR)/bootroms/gb/pb8.c
	gcc $< -o $@

$(ODIR)/bootroms/gb/%: $(SDIR)/bootroms/gb/%.asm $(ODIR)/bootroms/gb/gbmulator_logo.pb8 $(SDIR)/bootroms/gb/hardware.inc
	-@mkdir -p $(dir $@)
	rgbasm -I $(ODIR)/bootroms/gb/ -I $(SDIR)/bootroms/gb/ -o $@.tmp $<
	rgblink -o $@.tmp2 $@.tmp
	dd if=$@.tmp2 of=$@ count=1 bs=$(if $(findstring dmg,$@),256,2304)
	@rm $@.tmp $@.tmp2
#

$(ICONS): $(ICONDIR)/$(BIN).svg
	mkdir -p $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BIN).png,%,$@)
	magick $^ -background none -density 1200 -resize $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BIN).png,%,$@)/$(BIN).png

check: $(SDIR)
	cppcheck --enable=all --suppress=missingIncludeSystem -i $(SDIR)/platform/android/android-project -i $(SDIR)/platform/desktop/resources.c $(SDIR)

clean:
	rm -rf $(ODIR) web_build
	$(MAKE) -C test clean

cleaner: clean
	rm -rf $(BIN) $(SDIR)/platform/desktop/resources.c $(patsubst %/$(BIN).png,%,$(ICONS))
	$(MAKE) -C test cleaner
	cd $(SDIR)/platform/android/android-project && ./gradlew clean || true

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
	install -m 0644 $(ICONDIR)/128x128/$(BIN).png /usr/share/icons
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

-include $(OBJ:.o=.d)

.PHONY: all clean run install uninstall debug web debug_web android debug_android test
