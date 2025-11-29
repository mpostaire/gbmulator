SRC += \
	$(wildcard $(SDIR)/core/*.c) \
	$(wildcard $(SDIR)/core/gb/*.c) \
	$(wildcard $(SDIR)/core/gba/*.c) \
	$(wildcard $(SDIR)/core/gbprinter/*.c)

ODIR_STRUCTURE += $(ODIR)/core/gb $(ODIR)/core/gba $(ODIR)/core/gbprinter

$(ODIR)/core/gb/mmu.o: $(GB_BOOTROMS)
$(ODIR)/core/gba/bus.o: $(GBA_BOOTROM)
