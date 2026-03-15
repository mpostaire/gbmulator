OBJ := $(SRC:$(SDIR)/%.c=$(ODIR)/%.o)
BIN := $(ODIR)/../gbmulator.a

$(BIN): $(OBJ)
	ar rcs $@ $^
