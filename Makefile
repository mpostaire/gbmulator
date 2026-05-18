PLATFORM	?= desktop
BUILD_TYPE	?= release
ROOT_ODIR	?= build

SDIR := src

VERSION			:= $(shell git rev-parse --abbrev-ref HEAD)-$(shell git describe --always --tags --dirty)
COPYRIGHT_YEAR	:= $(shell date +%Y)

CC				:=	gcc
override CFLAGS +=	-std=gnu23 -I$(SDIR) \
					-DBUILD_TYPE_$(BUILD_TYPE) \
					-DVERSION=$(VERSION) \
					-DCOPYRIGHT_YEAR=$(COPYRIGHT_YEAR) \
					-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers

ifeq ($(BUILD_TYPE),debug)
override CFLAGS += -ggdb -O0 -fsanitize=undefined -fno-sanitize-recover=undefined
else ifeq ($(BUILD_TYPE),release)
override CFLAGS += -O3 -DNDEBUG -flto
else
$(error BUILD_TYPE='$(BUILD_TYPE)' is invalid. Choose one of [debug, release])
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
# 	$(MAKE) PLATFORM=test BUILD_TYPE=debug CFLAGS+=--coverage _test
# 	$(MAKE) PLATFORM=test BUILD_TYPE=debug "CFLAGS+=-DLOG_LEVEL=debug" _test
	$(MAKE) PLATFORM=test BUILD_TYPE=debug "CFLAGS+=-DLOG_LEVEL=warn" _test
# 	$(MAKE) PLATFORM=test BUILD_TYPE=debug "CFLAGS+=--coverage -DLOG_LEVEL=warn" _test
# 	rm -rf coverage
# 	mkdir coverage
# 	gcovr --gcov-ignore-parse-errors=all --html-nested=coverage/coverage.html

check: $(SDIR)
	cppcheck --enable=all --check-level=exhaustive --suppress=missingIncludeSystem -i $(SDIR)/platform/android -i $(SDIR)/platform/desktop/resources.c $(SDIR)

clean:
	rm -rf $(ROOT_ODIR)
	$(MAKE) PLATFORM=test _clean

.PHONY: all test clean check
