BOOTROM_ODIR ?= $(ROOT_ODIR)/bootroms
ODIR_STRUCTURE += $(BOOTROM_ODIR)/gb

GB_BOOTROMS := $(BOOTROM_ODIR)/gb/dmg_boot.bin $(BOOTROM_ODIR)/gb/cgb_boot.bin

$(BOOTROM_ODIR)/gb/gbmulator_logo.1bpp: $(SDIR)/bootroms/gb/gbmulator_logo.png
	rgbgfx -d 1 -Z -o $@ $<

$(BOOTROM_ODIR)/gb/gbmulator_logo.pb8: $(BOOTROM_ODIR)/gb/gbmulator_logo.1bpp $(BOOTROM_ODIR)/gb/pb8
	$(BOOTROM_ODIR)/gb/pb8 -l 384 $< $@

$(BOOTROM_ODIR)/gb/pb8: $(SDIR)/bootroms/gb/pb8.c
	gcc $< -o $@

$(BOOTROM_ODIR)/gb/%.bin: $(SDIR)/bootroms/gb/%.asm $(BOOTROM_ODIR)/gb/gbmulator_logo.pb8 $(SDIR)/bootroms/gb/hardware.inc
	rgbasm -I $(BOOTROM_ODIR)/gb -I $(SDIR)/bootroms/gb -o $@.tmp $<
	rgblink -o $@.tmp2 $@.tmp
	dd if=$@.tmp2 of=$@ count=1 bs=$(if $(findstring dmg,$@),256,2304)
	@rm $@.tmp $@.tmp2
