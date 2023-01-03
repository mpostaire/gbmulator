ODIR=out
EMU_SDIR=../src/emulator
CC=gcc
CFLAGS=-std=gnu11 -Wall -Wextra -O3 -I$(EMU_SDIR)
EXTRA_CFLAGS=$(shell pkg-config --cflags MagickWand)
LDLIBS=$(shell pkg-config --libs MagickWand)
BIN=tester

EMU_SRC=$(wildcard $(EMU_SDIR)/*.c)
EMU_OBJ=$(EMU_SRC:$(EMU_SDIR)/%.c=$(ODIR)/%.o)

TEST_ROMS=game-boy-test-roms-v5.1

all: $(ODIR)
	$(MAKE) tests.txt
	$(MAKE) $(BIN)
	./$(BIN) $(TEST_ROMS)

tests.txt: tests_parser.py $(TEST_ROMS)
	python3 $< $(TEST_ROMS)

$(TEST_ROMS):
	wget https://github.com/c-sp/gameboy-test-roms/releases/download/v5.1/game-boy-test-roms-v5.1.zip
	unzip game-boy-test-roms-v5.1.zip -d $(TEST_ROMS)
	rm -f game-boy-test-roms-v5.1.zip

$(BIN): $(EMU_OBJ) $(ODIR)/$(BIN).o
	$(CC) -o $@ $^ $(CFLAGS) $(EXTRA_CFLAGS) $(LDLIBS)

$(ODIR)/$(BIN).o: $(BIN).c
	$(CC) -o $@ -c $< $(CFLAGS) $(EXTRA_CFLAGS) $(LDLIBS) -MMD

$(ODIR)/%.o: $(EMU_SDIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD

$(ODIR):
	mkdir -p $@

clean:
	rm -rf $(BIN) out tests.txt

-include $(foreach d,$(ODIR),$d/*.d)

.PHONY: all clean
