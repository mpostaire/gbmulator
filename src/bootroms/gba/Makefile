ODIR_STRUCTURE:=$(ODIR)/bootroms/gba

all: $(ODIR_STRUCTURE) $(ODIR)/bootroms/gba/gba_bios.bin

$(ODIR_STRUCTURE):
	mkdir -p $@

$(ODIR)/bootroms/gba/gba_bios.bin: gba_bios.bin
	cp $^ $@
