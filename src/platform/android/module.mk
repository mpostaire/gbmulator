OBJ := $(SRC:$(SDIR)/%.c=$(ODIR)/%.o)
BIN := $(ODIR)/../gbmulator.a

CFLAGS += $(EXTRA_CFLAGS)

$(BIN): $(OBJ)
	ar rcs $@ $^
