PLATFORM  ?= desktop
DEBUG     ?= 0
ROOT_ODIR ?= build

SDIR := src

CC				:=	gcc
override CFLAGS +=	-std=gnu23 -I$(SDIR) \
					-DVERSION=$(shell git rev-parse --short HEAD) \
					-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers

BUILD_TYPE := debug

ifeq ($(DEBUG),1)
override CFLAGS += -ggdb -O0
else ifeq ($(DEBUG),2)
override CFLAGS += -ggdb -O0 -DDEBUG
else
override CFLAGS += -O3 -DNDEBUG
BUILD_TYPE := release
endif

ODIR := $(ROOT_ODIR)/$(BUILD_TYPE)/$(PLATFORM)

# This is needed because includes below may add recipes
.DEFAULT_GOAL := all

ODIR_STRUCTURE += $(ODIR)/platform/$(PLATFORM)

include src/bootroms/gb/module.mk
include src/bootroms/gba/module.mk
include src/core/module.mk
include src/platform/common/module.mk
include src/platform/$(PLATFORM)/module.mk

all: $(ODIR_STRUCTURE) $(BIN)

$(ODIR_STRUCTURE):
	mkdir -p $@

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS) -MMD -MP

-include $(OBJ:.o=.d)

test:
	$(MAKE) PLATFORM=test _test

check: $(SDIR)
	cppcheck --enable=all --check-level=exhaustive --suppress=missingIncludeSystem -i $(SDIR)/platform/android -i $(SDIR)/platform/desktop/resources.c $(SDIR)

clean:
	rm -rf $(ROOT_ODIR)
	$(MAKE) PLATFORM=test _clean

.PHONY: all test clean check
