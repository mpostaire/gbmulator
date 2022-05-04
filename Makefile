SDIR:=src
ODIR:=out
IDIR:=$(SDIR)
CFLAGS:=-std=gnu11 -Wall -O2 -I$(IDIR)
LDLIBS:=-lSDL2
CC:=gcc
EXEC:=gbmulator

# recusive wildcard that goes into all subdirectories
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC:=$(call rwildcard,$(SDIR),*.c)
OBJ:=$(SRC:$(SDIR)/%.c=$(ODIR)/%.o)

HEADERS:=$(call rwildcard,$(IDIR),*.h)
HEADERS:=$(HEADERS:$(IDIR)/%=$(ODIR)/%)
# ODIR and its subdirectories structure to mkdir if they don't exist
ODIR_STRUCTURE:=$(sort $(foreach d,$(OBJ) $(HEADERS),$(subst /$(lastword $(subst /, ,$d)),,$d)))

all: $(ODIR_STRUCTURE) $(EXEC)

debug: CFLAGS+=-g -Og -DDEBUG
debug: all

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

$(ODIR_STRUCTURE):
	mkdir -p $@

run: all
	./$(EXEC)

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

-include $(foreach d,$(ODIR_STRUCTURE),$d/*.d)

.PHONY: all clean run install debug
