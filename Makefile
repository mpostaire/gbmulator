SDIR:=src
ODIR:=build
IDIR:=$(SDIR)
CFLAGS:=-std=gnu23 -O3 -I$(IDIR) -DVERSION=$(shell git rev-parse --short HEAD) \
		-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type
LDLIBS:=
CC:=gcc
BIN:=gbmulator

# Important for android studio to add its extra flags
EXTRA_CFLAGS:=
CFLAGS+=$(EXTRA_CFLAGS)

all: desktop

debug: CFLAGS+=-ggdb -O0
# debug: CFLAGS+=-DDEBUG
debug: all

desktop: CFLAGS+=$(shell pkg-config --cflags gtk4 libadwaita-1 zlib manette-0.2 opengl openal gstreamer-1.0) -fanalyzer
desktop: LDLIBS+=$(shell pkg-config --libs gtk4 libadwaita-1 zlib manette-0.2 opengl openal gstreamer-1.0)
desktop: PLATFORM_ODIR:=$(ODIR)/desktop
desktop: common $(ICONS)
	@$(MAKE) -C $(SDIR)/platform/$@ ODIR=$(shell realpath -m $(PLATFORM_ODIR)) "CC=$(CC)" "CFLAGS=$(CFLAGS)" "LDLIBS=$(LDLIBS)"

web: CC:=emcc
web: CFLAGS+=-sUSE_ZLIB=1
web: PLATFORM_ODIR:=$(ODIR)/web
web: common
	@$(MAKE) -C $(SDIR)/platform/$@ ODIR=$(shell realpath -m $(PLATFORM_ODIR)) "CC=$(CC)" "CFLAGS=$(CFLAGS)" "LDLIBS=$(LDLIBS)"

debug_web: web
	emrun $(ODIR)/web/index.html

# test:
# 	$(MAKE) -C test

bootroms:
	@$(MAKE) -C $(SDIR)/bootroms/gb "ODIR=$(shell realpath -m $(ODIR))"
	@$(MAKE) -C $(SDIR)/bootroms/gba "ODIR=$(shell realpath -m $(ODIR))"

core: bootroms
	@$(MAKE) -C $(SDIR)/$@ ODIR=$(shell realpath -m $(PLATFORM_ODIR)) "CC=$(CC)" "CFLAGS=$(CFLAGS)"

common: core
	@$(MAKE) -C $(SDIR)/platform/$@ ODIR=$(shell realpath -m $(PLATFORM_ODIR)) "CC=$(CC)" "CFLAGS=$(CFLAGS)"

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

$(ICONS): $(ICONDIR)/$(BIN).svg
	mkdir -p $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BIN).png,%,$@)
	magick $^ -background none -density 1200 -resize $(patsubst $(ICONDIR)/%/$(BIN).png,%,$@) $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BIN).png,%,$@)/$(BIN).png

check: $(SDIR)
	cppcheck --enable=all --suppress=missingIncludeSystem -i $(SDIR)/platform/android/android-project -i $(SDIR)/platform/desktop/resources.c $(SDIR)

clean:
	rm -rf $(ODIR)
	$(MAKE) -C test clean

cleaner: clean
	rm -rf $(BIN) $(SDIR)/platform/desktop/resources.c $(patsubst %/$(BIN).png,%,$(ICONS))
	$(MAKE) -C test cleaner
	cd $(SDIR)/platform/android && ./gradlew clean || true

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

.PHONY: all clean cleaner install uninstall debug desktop web debug_web android test
