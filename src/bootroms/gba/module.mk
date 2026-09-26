BOOTROM_ODIR ?= $(ROOT_ODIR)/bootroms
ODIR_STRUCTURE += $(BOOTROM_ODIR)/gba

GBA_BOOTROM := $(BOOTROM_ODIR)/gba/gba_bios.bin

$(SDIR)/bootroms/gba/gba_bios.bin:
	dd if=/dev/zero of=$@ count=1 bs=16384

$(BOOTROM_ODIR)/gba/gba_bios.bin: $(SDIR)/bootroms/gba/gba_bios.bin
	cp $^ $@
