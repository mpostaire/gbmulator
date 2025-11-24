# Build boot roms (taken and modified from SameBoy emulator)

ODIR_STRUCTURE:=$(ODIR)/bootroms/gb

all: $(ODIR_STRUCTURE) $(ODIR)/bootroms/gb/dmg_boot.bin $(ODIR)/bootroms/gb/cgb_boot.bin

$(ODIR_STRUCTURE):
	mkdir -p $@

$(ODIR)/bootroms/gb/gbmulator_logo.1bpp: gbmulator_logo.png
	rgbgfx -d 1 -Z -o $@ $<

$(ODIR)/bootroms/gb/gbmulator_logo.pb8: $(ODIR)/bootroms/gb/gbmulator_logo.1bpp $(ODIR)/bootroms/gb/pb8
	$(ODIR)/bootroms/gb/pb8 -l 384 $< $@

$(ODIR)/bootroms/gb/pb8: pb8.c
	gcc $< -o $@

$(ODIR)/bootroms/gb/%.bin: %.asm $(ODIR)/bootroms/gb/gbmulator_logo.pb8 hardware.inc
	rgbasm -I $(ODIR)/bootroms/gb -o $@.tmp $<
	rgblink -o $@.tmp2 $@.tmp
	dd if=$@.tmp2 of=$@ count=1 bs=$(if $(findstring dmg,$@),256,2304)
	@rm $@.tmp $@.tmp2
