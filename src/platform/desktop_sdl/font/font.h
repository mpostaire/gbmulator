#pragma once

const byte_t font[0x5F][0x8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00 },
    { 0x00, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x14, 0x3E, 0x14, 0x3E, 0x14, 0x00 },
    { 0x10, 0x3E, 0x50, 0x3C, 0x12, 0x12, 0x7C, 0x10 },
    { 0x00, 0x20, 0x52, 0x24, 0x08, 0x12, 0x25, 0x02 },
    { 0x00, 0x18, 0x24, 0x18, 0x30, 0x4A, 0x3C, 0x00 },
    { 0x00, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x08, 0x10, 0x10, 0x10, 0x10, 0x08, 0x00 },
    { 0x00, 0x08, 0x04, 0x04, 0x04, 0x04, 0x08, 0x00 },
    { 0x00, 0x14, 0x08, 0x14, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10 },
    { 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 },
    { 0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },
    { 0x00, 0x3C, 0x46, 0x4A, 0x52, 0x62, 0x3C, 0x00 },
    { 0x00, 0x08, 0x18, 0x28, 0x08, 0x08, 0x3E, 0x00 },
    { 0x00, 0x3C, 0x42, 0x1C, 0x20, 0x40, 0x7E, 0x00 },
    { 0x00, 0x3C, 0x42, 0x1C, 0x02, 0x42, 0x3C, 0x00 },
    { 0x00, 0x44, 0x44, 0x7E, 0x04, 0x04, 0x04, 0x00 },
    { 0x00, 0x7C, 0x40, 0x7C, 0x02, 0x02, 0x7C, 0x00 },
    { 0x00, 0x3C, 0x40, 0x7C, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x7E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },
    { 0x00, 0x3C, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x3C, 0x42, 0x42, 0x3E, 0x02, 0x3C, 0x00 },
    { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00 },
    { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x10 },
    { 0x00, 0x00, 0x06, 0x18, 0x60, 0x18, 0x06, 0x00 },
    { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00 },
    { 0x00, 0x00, 0x60, 0x18, 0x06, 0x18, 0x60, 0x00 },
    { 0x00, 0x38, 0x44, 0x08, 0x10, 0x00, 0x10, 0x00 },
    { 0x00, 0x1C, 0x22, 0x4E, 0x4A, 0x2E, 0x10, 0x00 },
    { 0x00, 0x3C, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x7C, 0x42, 0x7C, 0x42, 0x42, 0x7C, 0x00 },
    { 0x00, 0x3C, 0x42, 0x40, 0x40, 0x42, 0x3C, 0x00 },
    { 0x00, 0x7C, 0x42, 0x42, 0x42, 0x42, 0x7C, 0x00 },
    { 0x00, 0x7E, 0x40, 0x7C, 0x40, 0x40, 0x7E, 0x00 },
    { 0x00, 0x7E, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x00 },
    { 0x00, 0x3C, 0x40, 0x4E, 0x42, 0x42, 0x3E, 0x00 },
    { 0x00, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00 },
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x48, 0x30, 0x00 },
    { 0x00, 0x48, 0x50, 0x60, 0x50, 0x48, 0x48, 0x00 },
    { 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7C, 0x00 },
    { 0x00, 0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x00 },
    { 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x7C, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x00 },
    { 0x00, 0x3C, 0x42, 0x42, 0x4A, 0x44, 0x3A, 0x00 },
    { 0x00, 0x7C, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x00 },
    { 0x00, 0x3E, 0x40, 0x3C, 0x02, 0x02, 0x7C, 0x00 },
    { 0x00, 0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },
    { 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x42, 0x42, 0x24, 0x24, 0x24, 0x18, 0x00 },
    { 0x00, 0x42, 0x42, 0x42, 0x5A, 0x66, 0x42, 0x00 },
    { 0x00, 0x44, 0x28, 0x10, 0x10, 0x28, 0x44, 0x00 },
    { 0x00, 0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x00 },
    { 0x00, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x7E, 0x00 },
    { 0x00, 0x18, 0x10, 0x10, 0x10, 0x10, 0x18, 0x00 },
    { 0x00, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 },
    { 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x18, 0x00 },
    { 0x00, 0x08, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00 },
    { 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x3C, 0x02, 0x3E, 0x42, 0x3C, 0x00 },
    { 0x00, 0x00, 0x40, 0x40, 0x7C, 0x42, 0x7C, 0x00 },
    { 0x00, 0x00, 0x3C, 0x42, 0x40, 0x42, 0x3C, 0x00 },
    { 0x00, 0x00, 0x02, 0x02, 0x3E, 0x42, 0x3E, 0x00 },
    { 0x00, 0x00, 0x3C, 0x42, 0x7C, 0x40, 0x3C, 0x00 },
    { 0x00, 0x00, 0x08, 0x10, 0x38, 0x10, 0x10, 0x00 },
    { 0x00, 0x00, 0x3E, 0x42, 0x3E, 0x02, 0x3C, 0x00 },
    { 0x00, 0x00, 0x40, 0x40, 0x78, 0x44, 0x44, 0x00 },
    { 0x00, 0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x00 },
    { 0x00, 0x00, 0x08, 0x00, 0x18, 0x08, 0x08, 0x38 },
    { 0x00, 0x00, 0x10, 0x10, 0x14, 0x18, 0x14, 0x00 },
    { 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x18, 0x00 },
    { 0x00, 0x00, 0x74, 0x4A, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x00, 0x7C, 0x42, 0x42, 0x42, 0x42, 0x00 },
    { 0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    { 0x00, 0x00, 0x7C, 0x42, 0x7C, 0x40, 0x40, 0x40 },
    { 0x00, 0x00, 0x3E, 0x42, 0x3E, 0x02, 0x02, 0x02 },
    { 0x00, 0x00, 0x2E, 0x31, 0x20, 0x20, 0x20, 0x00 },
    { 0x00, 0x00, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x00 },
    { 0x00, 0x00, 0x10, 0x38, 0x10, 0x10, 0x08, 0x00 },
    { 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x3E, 0x00 },
    { 0x00, 0x00, 0x42, 0x42, 0x24, 0x24, 0x18, 0x00 },
    { 0x00, 0x00, 0x42, 0x42, 0x42, 0x4A, 0x34, 0x00 },
    { 0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 },
    { 0x00, 0x00, 0x44, 0x28, 0x10, 0x20, 0x40, 0x00 },
    { 0x00, 0x00, 0x7C, 0x08, 0x10, 0x20, 0x7C, 0x00 },
    { 0x00, 0x08, 0x10, 0x30, 0x30, 0x10, 0x08, 0x00 },
    { 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },
    { 0x00, 0x10, 0x08, 0x0C, 0x0C, 0x08, 0x10, 0x00 },
    { 0x00, 0x00, 0x00, 0x10, 0x2A, 0x04, 0x00, 0x00 }
};
