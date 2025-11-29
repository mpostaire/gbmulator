UI_RESOURCES := $(ODIR)/platform/$(PLATFORM)/resources
BINNAME := gbmulator

SRC += $(wildcard $(SDIR)/platform/$(PLATFORM)/*.c)
OBJ := $(SRC:$(SDIR)/%.c=$(ODIR)/%.o) $(UI_RESOURCES).o
BIN := $(ODIR)/$(BINNAME)

CFLAGS += $(shell pkg-config --cflags gtk4 libadwaita-1 zlib manette-0.2 opengl openal gstreamer-1.0) -fanalyzer
LDLIBS += $(shell pkg-config --libs gtk4 libadwaita-1 zlib manette-0.2 opengl openal gstreamer-1.0)

$(BIN): $(OBJ)
	$(CC) -o $(BIN) $^ $(CFLAGS) $(LDLIBS)

$(UI_RESOURCES).c: $(SDIR)/platform/$(PLATFORM)/ui/$(BINNAME).gresource.xml $(wildcard $(SDIR)/platform/$(PLATFORM)/ui/*.ui)
	glib-compile-resources $< --target=$@ --generate-source --sourcedir=$(SDIR)/platform/$(PLATFORM)

ICONDIR  := $(ODIR)/icons

ODIR_STRUCTURE += $(ICONDIR)

ICONS= \
	$(ICONDIR)/192x192/$(BINNAME).png \
	$(ICONDIR)/144x144/$(BINNAME).png \
	$(ICONDIR)/128x128/$(BINNAME).png \
	$(ICONDIR)/96x96/$(BINNAME).png \
	$(ICONDIR)/72x72/$(BINNAME).png \
	$(ICONDIR)/64x64/$(BINNAME).png \
	$(ICONDIR)/48x48/$(BINNAME).png \
	$(ICONDIR)/32x32/$(BINNAME).png \
	$(ICONDIR)/16x16/$(BINNAME).png

$(ICONS): images/icons/$(BINNAME).svg
	mkdir -p $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BINNAME).png,%,$@)
	magick $^ -background none -density 1200 -resize $(patsubst $(ICONDIR)/%/$(BINNAME).png,%,$@) $(ICONDIR)/$(patsubst $(ICONDIR)/%/$(BINNAME).png,%,$@)/$(BINNAME).png

install: $(ODIR_STRUCTURE) $(ICONS) $(BIN)
	install -m 0755 $(BIN) /usr/bin
	install -m 0644 $(SDIR)/platform/desktop/$(BINNAME).xml /usr/share/mime/application
	install -m 0644 $(SDIR)/platform/desktop/$(BINNAME).desktop /usr/share/applications
	install -m 0644 $(ICONDIR)/$(BINNAME).svg /usr/share/icons/hicolor/scalable/apps
	install -m 0644 $(ICONDIR)/192x192/$(BINNAME).png /usr/share/icons/hicolor/192x192/apps
	install -m 0644 $(ICONDIR)/128x128/$(BINNAME).png /usr/share/icons/hicolor/128x128/apps
	install -m 0644 $(ICONDIR)/96x96/$(BINNAME).png /usr/share/icons/hicolor/96x96/apps
	install -m 0644 $(ICONDIR)/72x72/$(BINNAME).png /usr/share/icons/hicolor/72x72/apps
	install -m 0644 $(ICONDIR)/64x64/$(BINNAME).png /usr/share/icons/hicolor/64x64/apps
	install -m 0644 $(ICONDIR)/48x48/$(BINNAME).png /usr/share/icons/hicolor/48x48/apps
	install -m 0644 $(ICONDIR)/32x32/$(BINNAME).png /usr/share/icons/hicolor/32x32/apps
	install -m 0644 $(ICONDIR)/16x16/$(BINNAME).png /usr/share/icons/hicolor/16x16/apps
	install -m 0644 $(ICONDIR)/128x128/$(BINNAME).png /usr/share/icons
	gtk-update-icon-cache
	update-desktop-database

uninstall:
	rm -f /usr/bin/$(BINNAME)
	rm -f /usr/share/applications/$(BINNAME).desktop
	rm -f /usr/share/icons/hicolor/scalable/apps/$(BINNAME).svg
	rm -f /usr/share/icons/hicolor/{192x192,128x128,96x96,72x72,64x64,48x48,32x32,16x16}/apps/$(BINNAME).png
	rm -f /usr/share/mime/application/$(BINNAME).xml
	gtk-update-icon-cache
	update-desktop-database

.PHONY: install uninstall
