#include "gb_priv.h"

static inline uint8_t gdma_hdma_copy_step(gb_t *gb) {
    gb_mmu_t *mmu = &gb->mmu;

    // normal speed: one step is 4 cycles -> 2 cycles to copy 1 byte -> copy 2 bytes from src to dest
    // double speed: one step is 8 cycles -> 4 cycles to copy 1 byte -> copy 1 byte from src to dest
    for (uint8_t i = 0; i < !IS_DOUBLE_SPEED(gb) + 1; i++) {
        uint8_t data = mmu_read_io_src(gb, mmu->hdma.src_address++, IO_SRC_GDMA_HDMA);
        mmu_write_io_src(gb, mmu->hdma.dest_address++, data, IO_SRC_GDMA_HDMA);

        // printf("copy %x from %x to %x\n", data, mmu->hdma.src_address - 1, mmu->hdma.dest_address - 1);

        if (mmu->hdma.dest_address >= MMU_ERAM) { // vram overflow stops prematurely the transfer
            mmu->hdma.progress = 0;
            break;
        }
    }

    if (!(mmu->hdma.src_address & 0x000F)) { // a block of 0x10 bytes has been copied
        mmu->io_registers[IO_HDMA5] = (GBC_GDMA_HDMA_LENGTH(mmu) - 1) & 0x7F;
        mmu->hdma.progress--;

        // HDMA src and dest registers need to increase as the transfer progresses
        mmu->io_registers[IO_HDMA1] = mmu->hdma.src_address >> 8;
        mmu->io_registers[IO_HDMA2] = mmu->hdma.src_address & 0xF0;
        mmu->io_registers[IO_HDMA3] = (mmu->hdma.dest_address >> 8) & 0x1F;
        mmu->io_registers[IO_HDMA4] = mmu->hdma.dest_address & 0xF0;

        return 1;
    }

    return 0;
}

static inline void gdma_hdma_step(gb_t *gb) {
    gb_mmu_t *mmu = &gb->mmu;

    if (mmu->hdma.progress == 0)
        return;

    if (mmu->hdma.initializing) {
        mmu->hdma.initializing = 0;
        return;
    }

    switch (mmu->hdma.type) {
    case GDMA:
        // cpu locked as soon as GDMA was requested, don't need to lock it here
        gdma_hdma_copy_step(gb);

        if (mmu->hdma.progress == 0) { // finished copying?
            mmu->hdma.lock_cpu          = 0;
            mmu->io_registers[IO_HDMA5] = 0xFF;
        }
        break;
    case HDMA:
        if (!mmu->hdma.allow_hdma_block || gb->cpu.halt) // HDMA can't work while cpu is halted
            return;

        // locks cpu while transfer of 0x10 byte block in progress
        mmu->hdma.lock_cpu = 1;

        if (gdma_hdma_copy_step(gb)) {
            // one block of 0x10 bytes has been copied
            mmu->hdma.lock_cpu         = 0;
            mmu->hdma.allow_hdma_block = 0;

            if (mmu->hdma.progress == 0) // finished copying?
                mmu->io_registers[IO_HDMA5] = 0xFF;
        }
        break;
    }
}

static inline void oam_dma_step(gb_t *gb) {
    gb_mmu_t *mmu = &gb->mmu;

    // run oam dma initializations procedure if there are any starting oam dma
    for (unsigned int i = 0; mmu->oam_dma.starting_count > 0 && i < sizeof(mmu->oam_dma.starting_statuses); i++) {
        switch (mmu->oam_dma.starting_statuses[i]) {
        case OAM_DMA_NO_INIT:
            break;
        case OAM_DMA_INIT_BEGIN:
            mmu->oam_dma.starting_statuses[i] = OAM_DMA_INIT_PENDING;
            break;
        case OAM_DMA_INIT_PENDING:
            mmu->oam_dma.starting_statuses[i] = OAM_DMA_STARTING;
            break;
        case OAM_DMA_STARTING:
            mmu->oam_dma.starting_statuses[i] = OAM_DMA_NO_INIT;
            mmu->oam_dma.progress             = 0;
            mmu->oam_dma.src_address          = mmu->io_registers[IO_DMA] << 8;
            mmu->oam_dma.starting_count--;
            break;
        }
    }

    if (IS_OAM_DMA_RUNNING(mmu)) {
        // oam dma step (no need to call mmu_write_io_src() because dest can only be inside OAM and there are no access restrictions)
        mmu->oam[mmu->oam_dma.progress] = mmu_read_io_src(gb, mmu->oam_dma.src_address + mmu->oam_dma.progress, IO_SRC_OAM_DMA);
        mmu->oam_dma.progress++;
    }
}

void dma_step(gb_t *gb) {
    oam_dma_step(gb);
    if (gb->base->opts.mode == GBMULATOR_MODE_GBC)
        gdma_hdma_step(gb);
}
