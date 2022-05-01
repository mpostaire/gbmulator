SDIR:=src
ODIR:=out
IDIR:=./
CFLAGS:=-std=gnu99 -Wall -O2 -I$(IDIR)
LDLIBS:=-lSDL2
CC:=gcc
EXEC:=gbmulator

# recusive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SRC:=$(call rwildcard,$(SDIR),*.c)
OBJ:=$(SRC:$(SDIR)/%.c=$(ODIR)/%.o)

# ODIR and subdirectories structure to mkdir if they don't exist
odirs_structure:=$(sort $(foreach d,$(OBJ),$(subst /$(lastword $(subst /, ,$d)),,$d)))

all: $(odirs_structure) $(EXEC)

debug: CFLAGS+=-g -Og -D DEBUG
debug: all

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

$(odirs_structure):
	mkdir -p $@

run: all
	./$(EXEC) "roms/Pokemon Red.gb"

check: $(SDIR)/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -rf $(ODIR)

cleaner: clean
	rm -f $(EXEC)

install:
	install -m 0755 $(EXEC) /usr/bin

uninstall:
	rm -f /usr/bin/$(EXEC)

-include $(ODIR)/*.d

.PHONY: all clean run install debug
