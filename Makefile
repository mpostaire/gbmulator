SDIR=src
ODIR=out
IDIR=$(SDIR)/headers
CFLAGS=-std=gnu99 -Wall -I$(IDIR)
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

debug: CFLAGS+=-g -D DEBUG
debug: clean all

check: $(SDIR)/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -f $(OBJ) $(ODIR)/*.d

cleaner: clean
	rm -f $(EXEC)

-include $(ODIR)/*.d

.PHONY: all clean run
