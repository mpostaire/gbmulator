SRC += $(wildcard $(SDIR)/platform/$(PLATFORM)/*.c)
OBJ := $(SRC:$(SDIR)/%.c=$(ODIR)/%.o)
BIN := $(ODIR)/tester

CFLAGS += $(shell pkg-config --cflags zlib opengl openal MagickWand) -fanalyzer -O0 -ggdb3 -DDISABLE_COLOR_CORRECTION
LDLIBS += $(shell pkg-config --libs zlib opengl openal MagickWand) -lpthread

# CFLAGS += -Wl,--wrap=_gba_bus_read_byte -Wl,--wrap=_gba_bus_read_half -Wl,--wrap=_gba_bus_read_word -Wl,--wrap=_gba_bus_write_byte -Wl,--wrap=_gba_bus_write_half -Wl,--wrap=_gba_bus_write_word

TEST_ROMS=$(SDIR)/platform/$(PLATFORM)/test_roms

_test: all
	cp $(SDIR)/platform/$(PLATFORM)/results/summary.txt $(SDIR)/platform/$(PLATFORM)/results/summary_old.txt || true
	rm -rf $(SDIR)/platform/$(PLATFORM)/tester
	ln -s $(PWD)/$(BIN) $(SDIR)/platform/$(PLATFORM)
	cd $(SDIR)/platform/$(PLATFORM) && ./tester test_roms
	sort -o $(SDIR)/platform/$(PLATFORM)/results/summary.txt $(SDIR)/platform/$(PLATFORM)/results/summary.txt
	python3 $(SDIR)/platform/$(PLATFORM)/compare_results.py || true

_clean:
	rm -rf $(BIN) $(SDIR)/platform/$(PLATFORM)/tester $(SDIR)/platform/$(PLATFORM)/tests.txt $(SDIR)/platform/$(PLATFORM)/results/summary.txt.tmp

$(SDIR)/platform/$(PLATFORM)/tests.txt: $(SDIR)/platform/$(PLATFORM)/tests_generator.py $(TEST_ROMS)
	cd $(dir $<) && python3 $(notdir $<) $(notdir $(TEST_ROMS))

$(BIN): $(SDIR)/platform/$(PLATFORM)/tests.txt $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(CFLAGS) $(LDLIBS)

$(TEST_ROMS):
	rm -rf $(SDIR)/platform/$(PLATFORM)/game-boy-test-roms-v6.0.zip $(SDIR)/platform/$(PLATFORM)/docboy-test-suite.zip $(SDIR)/platform/$(PLATFORM)/docboy-test-suite-master
	wget https://github.com/c-sp/gameboy-test-roms/releases/download/v6.0/game-boy-test-roms-v6.0.zip -O $(SDIR)/platform/$(PLATFORM)/game-boy-test-roms-v6.0.zip
	wget https://github.com/Docheinstein/docboy-test-suite/archive/refs/heads/master.zip -O $(SDIR)/platform/$(PLATFORM)/docboy-test-suite.zip
	unzip $(SDIR)/platform/$(PLATFORM)/game-boy-test-roms-v6.0.zip -d $(TEST_ROMS)
	unzip $(SDIR)/platform/$(PLATFORM)/docboy-test-suite.zip -d $(SDIR)/platform/$(PLATFORM)
	$(MAKE) -j -C $(SDIR)/platform/$(PLATFORM)/docboy-test-suite-master
	mv $(SDIR)/platform/$(PLATFORM)/docboy-test-suite-master/roms $(TEST_ROMS)/docboy-test-suite
	wget -P $(TEST_ROMS)/other/windesync-validate https://github.com/nitro2k01/little-things-gb/releases/download/Win-desync-v1.0/windesync-validate.gb
	wget -P $(TEST_ROMS)/other/windesync-validate https://raw.githubusercontent.com/nitro2k01/little-things-gb/main/windesync-validate/images/windesync-reference-sgb.png
	convert $(TEST_ROMS)/other/windesync-validate/windesync-reference-sgb.png \
		-fill "#AAAAAA" -opaque "#BFBFBF" \
		-fill "#000000" -opaque "#3F3F3F" \
		$(TEST_ROMS)/other/windesync-validate/windesync-reference-sgb.png
	convert $(SDIR)/platform/$(PLATFORM)/docboy-test-suite-master/results/success.png \
		-fill "#FFFFFF" -opaque "#839500" \
		-fill "#000000" -opaque "#104000" \
		$(TEST_ROMS)/docboy-test-suite/success.png
	rm -rf $(SDIR)/platform/$(PLATFORM)/game-boy-test-roms-v6.0.zip $(SDIR)/platform/$(PLATFORM)/docboy-test-suite.zip $(SDIR)/platform/$(PLATFORM)/docboy-test-suite-master

.PHONY: _test _clean
