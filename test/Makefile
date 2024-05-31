ODIR=../out/test/core
EMU_SDIR=../src/core
CC=gcc
CFLAGS=-std=gnu11 -Wall -Wextra -O3 -I$(EMU_SDIR)
EXTRA_CFLAGS=$(shell pkg-config --cflags zlib MagickWand)
LDLIBS=$(shell pkg-config --libs zlib MagickWand)
BIN=tester

EMU_SRC=$(wildcard $(EMU_SDIR)/*.c)
EMU_OBJ=$(EMU_SRC:$(EMU_SDIR)/%.c=$(ODIR)/%.o)

TEST_ROMS=test_roms

all: $(ODIR)
	$(MAKE) tests.txt
	$(MAKE) $(BIN)
	cp results/summary.txt results/summary_old.txt || true
	./$(BIN) $(TEST_ROMS)
	python3 compare_results.py || true

tests.txt: tests_generator.py $(TEST_ROMS)
	python3 $< $(TEST_ROMS)

$(TEST_ROMS):
	wget https://github.com/c-sp/gameboy-test-roms/releases/download/v6.0/game-boy-test-roms-v6.0.zip
	unzip game-boy-test-roms-v6.0.zip -d $(TEST_ROMS)
	rm -f game-boy-test-roms-v6.0.zip
	wget -P $(TEST_ROMS)/other/windesync-validate https://github.com/nitro2k01/little-things-gb/releases/download/Win-desync-v1.0/windesync-validate.gb
	wget -P $(TEST_ROMS)/other/windesync-validate https://raw.githubusercontent.com/nitro2k01/little-things-gb/main/windesync-validate/images/windesync-reference-sgb.png
	convert $(TEST_ROMS)/other/windesync-validate/windesync-reference-sgb.png \
		-fill "#AAAAAA" -opaque "#BFBFBF" \
		-fill "#000000" -opaque "#3F3F3F" \
		$(TEST_ROMS)/other/windesync-validate/windesync-reference-sgb.png

$(BIN): $(EMU_OBJ) $(ODIR)/$(BIN).o
	$(CC) -o $@ $^ $(CFLAGS) $(EXTRA_CFLAGS) $(LDLIBS)

$(ODIR)/$(BIN).o: $(BIN).c
	$(CC) -o $@ -c $< $(CFLAGS) $(EXTRA_CFLAGS) $(LDLIBS) -MMD

$(ODIR)/%.o: $(EMU_SDIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD

$(ODIR):
	mkdir -p $@

clean:
	rm -rf $(BIN) out tests.txt results/summary.txt.tmp

-include $(foreach d,$(ODIR),$d/*.d)

.PHONY: all clean
