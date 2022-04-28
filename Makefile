SDIR=src
ODIR=out
IDIR=$(SDIR)/headers
CFLAGS=-std=gnu99 -Wall -O2 -I$(IDIR)
LDLIBS=-lSDL2
CC=gcc
EXEC=gbmulator
SRC= $(wildcard $(SDIR)/*.c)
OBJ= $(SRC:$(SDIR)/%.c=$(ODIR)/%.o)

all: $(ODIR) $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

$(ODIR):
	mkdir $@

run: all
	./$(EXEC) "roms/Pokemon Red.gb"

debug: CFLAGS+=-g -Og
debug: clean all

check: $(SDIR)/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -f $(OBJ) $(ODIR)/*.d

cleaner: clean
	rm -f $(EXEC)

install:
	install -m 0755 $(EXEC) /usr/bin

uninstall:
	rm -f /usr/bin/$(EXEC)

-include $(ODIR)/*.d

.PHONY: all clean run install debug
