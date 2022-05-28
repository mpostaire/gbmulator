/*
 * SAMEBOY boot roms
 *
 * MIT License
 *
 * Copyright (c) 2015-2021 Lior Halphon
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "types.h"
#include "boot.h"

const byte_t dmg_boot[0x100] = {
    0x31, 0xFE, 0xFF, 0x21, 0x00, 0x80, 0x22, 0xCB, 0x6C, 0x28, 0xFB, 0x3E, 0x80, 0xE0, 0x26, 0xE0,
    0x11, 0x3E, 0xF3, 0xE0, 0x12, 0xE0, 0x25, 0x3E, 0x77, 0xE0, 0x24, 0x3E, 0x54, 0xE0, 0x47, 0x11,
    0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0x47, 0xCD, 0xA2, 0x00, 0xCD, 0xA2, 0x00, 0x13, 0x7B, 0xEE,
    0x34, 0x20, 0xF2, 0x11, 0xD1, 0x00, 0x0E, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x0D, 0x20, 0xF9, 0x3E,
    0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20, 0xF9,
    0x2E, 0x0F, 0x18, 0xF5, 0x3E, 0x1E, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x16, 0x89, 0x0E, 0x0F,
    0xCD, 0xB7, 0x00, 0x7A, 0xCB, 0x2F, 0xCB, 0x2F, 0xE0, 0x42, 0x7A, 0x81, 0x57, 0x79, 0xFE, 0x08,
    0x20, 0x04, 0x3E, 0xA8, 0xE0, 0x47, 0x0D, 0x20, 0xE7, 0x3E, 0xFC, 0xE0, 0x47, 0x3E, 0x83, 0xCD,
    0xCA, 0x00, 0x06, 0x05, 0xCD, 0xC3, 0x00, 0x3E, 0xC1, 0xCD, 0xCA, 0x00, 0x06, 0x3C, 0xCD, 0xC3,
    0x00, 0x21, 0xB0, 0x01, 0xE5, 0xF1, 0x21, 0x4D, 0x01, 0x01, 0x13, 0x00, 0x11, 0xD8, 0x00, 0xC3,
    0xFE, 0x00, 0x3E, 0x04, 0x0E, 0x00, 0xCB, 0x20, 0xF5, 0xCB, 0x11, 0xF1, 0xCB, 0x11, 0x3D, 0x20,
    0xF5, 0x79, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xE5, 0x21, 0x0F, 0xFF, 0xCB, 0x86, 0xCB, 0x46, 0x28,
    0xFC, 0xE1, 0xC9, 0xCD, 0xB7, 0x00, 0x05, 0x20, 0xFA, 0xC9, 0xE0, 0x13, 0x3E, 0x87, 0xE0, 0x14,
    0xC9, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x50
};

const byte_t cgb_boot[0x900] = {
    0x31, 0xFE, 0xFF, 0xCD, 0x26, 0x06, 0x26, 0xFE, 0x0E, 0xA0, 0x22, 0x0D,
    0x20, 0xFC, 0x0E, 0x10, 0x21, 0x30, 0xFF, 0x22, 0x2F, 0x0D, 0x20, 0xFB,
    0xE0, 0xC1, 0xE0, 0x80, 0x3E, 0x80, 0xE0, 0x26, 0xE0, 0x11, 0x3E, 0xF3,
    0xE0, 0x12, 0xE0, 0x25, 0x3E, 0x77, 0xE0, 0x24, 0x3E, 0xFC, 0xE0, 0x47,
    0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0x47, 0xCD, 0xF1, 0x05, 0x13,
    0x7B, 0xFE, 0x34, 0x20, 0xF5, 0xCD, 0xAE, 0x06, 0x3E, 0x01, 0xE0, 0x4F,
    0xCD, 0x26, 0x06, 0xCD, 0x53, 0x06, 0x06, 0x03, 0x21, 0xC2, 0x98, 0x16,
    0x03, 0x3E, 0x08, 0x0E, 0x10, 0xCD, 0x86, 0x00, 0xF5, 0xD6, 0x20, 0xD6,
    0x03, 0x30, 0x06, 0xC6, 0x20, 0xCD, 0x86, 0x00, 0x0D, 0xF1, 0x82, 0x0D,
    0x20, 0xEB, 0xD6, 0x2C, 0xD5, 0x11, 0x10, 0x00, 0x19, 0xD1, 0x05, 0x20,
    0xDE, 0x15, 0x28, 0x17, 0x15, 0x3E, 0x38, 0x2E, 0xA7, 0x01, 0x07, 0x01,
    0x18, 0xD3, 0xF5, 0x3E, 0x01, 0xE0, 0x4F, 0x36, 0x08, 0xAF, 0xE0, 0x4F,
    0xF1, 0x22, 0xC9, 0x11, 0xE1, 0x05, 0x0E, 0x08, 0x21, 0x81, 0xFF, 0xAF,
    0x2F, 0x22, 0x22, 0x1A, 0x1C, 0xF6, 0x20, 0x47, 0x1A, 0x1D, 0xF6, 0x84,
    0x1F, 0xCB, 0x18, 0x70, 0x2C, 0x22, 0xAF, 0x22, 0x22, 0x1A, 0x1C, 0x22,
    0x1A, 0x1C, 0x22, 0xAF, 0x0D, 0x20, 0xE1, 0xCD, 0x18, 0x08, 0x3E, 0x91,
    0xE0, 0x40, 0xCD, 0xBB, 0x06, 0x3E, 0x30, 0xE0, 0xC2, 0x06, 0x04, 0xCD,
    0x15, 0x06, 0x3E, 0x83, 0xCD, 0x1F, 0x06, 0x06, 0x05, 0xCD, 0x15, 0x06,
    0x3E, 0xC1, 0xCD, 0x1F, 0x06, 0xCD, 0x3F, 0x08, 0xCD, 0x09, 0x06, 0x21,
    0xC2, 0xFF, 0x35, 0x20, 0xF4, 0xCD, 0xE4, 0x06, 0x18, 0x10, 0xD0, 0x00,
    0x98, 0xA0, 0x12, 0xD0, 0x00, 0x80, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x16, 0x36,
    0xD1, 0xDB, 0xF2, 0x3C, 0x8C, 0x92, 0x3D, 0x5C, 0x58, 0xC9, 0x3E, 0x70,
    0x1D, 0x59, 0x69, 0x19, 0x35, 0xA8, 0x14, 0xAA, 0x75, 0x95, 0x99, 0x34,
    0x6F, 0x15, 0xFF, 0x97, 0x4B, 0x90, 0x17, 0x10, 0x39, 0xF7, 0xF6, 0xA2,
    0x49, 0x4E, 0x43, 0x68, 0xE0, 0x8B, 0xF0, 0xCE, 0x0C, 0x29, 0xE8, 0xB7,
    0x86, 0x9A, 0x52, 0x01, 0x9D, 0x71, 0x9C, 0xBD, 0x5D, 0x6D, 0x67, 0x3F,
    0x6B, 0xB3, 0x46, 0x28, 0xA5, 0xC6, 0xD3, 0x27, 0x61, 0x18, 0x66, 0x6A,
    0xBF, 0x0D, 0xF4, 0xB3, 0x46, 0x28, 0xA5, 0xC6, 0xD3, 0x27, 0x61, 0x18,
    0x66, 0x6A, 0xBF, 0x0D, 0xF4, 0xB3, 0x00, 0x04, 0x05, 0x23, 0x22, 0x03,
    0x1F, 0x0F, 0x0A, 0x05, 0x13, 0x24, 0x87, 0x25, 0x1E, 0x2C, 0x15, 0x20,
    0x1F, 0x14, 0x05, 0x21, 0x0D, 0x0E, 0x05, 0x1D, 0x05, 0x12, 0x09, 0x03,
    0x02, 0x1A, 0x19, 0x19, 0x29, 0x2A, 0x1A, 0x2D, 0x2A, 0x2D, 0x24, 0x26,
    0x9A, 0x2A, 0x1E, 0x29, 0x22, 0x22, 0x05, 0x2A, 0x06, 0x05, 0x21, 0x19,
    0x2A, 0x2A, 0x28, 0x02, 0x10, 0x19, 0x2A, 0x2A, 0x05, 0x00, 0x27, 0x24,
    0x16, 0x19, 0x06, 0x20, 0x0C, 0x24, 0x0B, 0x27, 0x12, 0x27, 0x18, 0x1F,
    0x32, 0x11, 0x2E, 0x06, 0x1B, 0x00, 0x2F, 0x29, 0x29, 0x00, 0x00, 0x13,
    0x22, 0x17, 0x12, 0x1D, 0x42, 0x45, 0x46, 0x41, 0x41, 0x52, 0x42, 0x45,
    0x4B, 0x45, 0x4B, 0x20, 0x52, 0x2D, 0x55, 0x52, 0x41, 0x52, 0x20, 0x49,
    0x4E, 0x41, 0x49, 0x4C, 0x49, 0x43, 0x45, 0x20, 0x52, 0x20, 0x20, 0xE8,
    0x90, 0x90, 0x90, 0xA0, 0xA0, 0xA0, 0xC0, 0xC0, 0xC0, 0x48, 0x48, 0x48,
    0x00, 0x00, 0x00, 0xD8, 0xD8, 0xD8, 0x28, 0x28, 0x28, 0x60, 0x60, 0x60,
    0xD0, 0xD0, 0xD0, 0x80, 0x40, 0x40, 0x20, 0xE0, 0xE0, 0x20, 0x10, 0x10,
    0x18, 0x20, 0x20, 0x20, 0xE8, 0xE8, 0xE0, 0x20, 0xE0, 0x10, 0x88, 0x10,
    0x80, 0x80, 0x40, 0x20, 0x20, 0x38, 0x20, 0x20, 0x90, 0x20, 0x20, 0xA0,
    0x98, 0x98, 0x48, 0x1E, 0x1E, 0x58, 0x88, 0x88, 0x10, 0x20, 0x20, 0x10,
    0x20, 0x20, 0x18, 0xE0, 0xE0, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x08,
    0x90, 0xB0, 0x90, 0xA0, 0xB0, 0xA0, 0xC0, 0xB0, 0xC0, 0x80, 0xB0, 0x40,
    0x88, 0x20, 0x68, 0xDE, 0x00, 0x70, 0xDE, 0x20, 0x78, 0x98, 0xB6, 0x48,
    0x80, 0xE0, 0x50, 0x20, 0xB8, 0xE0, 0x88, 0xB0, 0x10, 0x20, 0x00, 0x10,
    0x20, 0xE0, 0x18, 0xE0, 0x18, 0x00, 0x18, 0xE0, 0x20, 0xA8, 0xE0, 0x20,
    0x18, 0xE0, 0x00, 0xC8, 0x18, 0xE0, 0x00, 0xE0, 0x40, 0x20, 0x18, 0xE0,
    0xE0, 0x18, 0x30, 0x20, 0xE0, 0xE8, 0xF0, 0xF0, 0xF0, 0xF8, 0xF8, 0xF8,
    0xE0, 0x20, 0x08, 0x00, 0x00, 0x10, 0xFF, 0x7F, 0xBF, 0x32, 0xD0, 0x00,
    0x00, 0x00, 0x9F, 0x63, 0x79, 0x42, 0xB0, 0x15, 0xCB, 0x04, 0xFF, 0x7F,
    0x31, 0x6E, 0x4A, 0x45, 0x00, 0x00, 0xFF, 0x7F, 0xEF, 0x1B, 0x00, 0x02,
    0x00, 0x00, 0xFF, 0x7F, 0x1F, 0x42, 0xF2, 0x1C, 0x00, 0x00, 0xFF, 0x7F,
    0x94, 0x52, 0x4A, 0x29, 0x00, 0x00, 0xFF, 0x7F, 0xFF, 0x03, 0x2F, 0x01,
    0x00, 0x00, 0xFF, 0x7F, 0xEF, 0x03, 0xD6, 0x01, 0x00, 0x00, 0xFF, 0x7F,
    0xB5, 0x42, 0xC8, 0x3D, 0x00, 0x00, 0x74, 0x7E, 0xFF, 0x03, 0x80, 0x01,
    0x00, 0x00, 0xFF, 0x67, 0xAC, 0x77, 0x13, 0x1A, 0x6B, 0x2D, 0xD6, 0x7E,
    0xFF, 0x4B, 0x75, 0x21, 0x00, 0x00, 0xFF, 0x53, 0x5F, 0x4A, 0x52, 0x7E,
    0x00, 0x00, 0xFF, 0x4F, 0xD2, 0x7E, 0x4C, 0x3A, 0xE0, 0x1C, 0xED, 0x03,
    0xFF, 0x7F, 0x5F, 0x25, 0x00, 0x00, 0x6A, 0x03, 0x1F, 0x02, 0xFF, 0x03,
    0xFF, 0x7F, 0xFF, 0x7F, 0xDF, 0x01, 0x12, 0x01, 0x00, 0x00, 0x1F, 0x23,
    0x5F, 0x03, 0xF2, 0x00, 0x09, 0x00, 0xFF, 0x7F, 0xEA, 0x03, 0x1F, 0x01,
    0x00, 0x00, 0x9F, 0x29, 0x1A, 0x00, 0x0C, 0x00, 0x00, 0x00, 0xFF, 0x7F,
    0x7F, 0x02, 0x1F, 0x00, 0x00, 0x00, 0xFF, 0x7F, 0xE0, 0x03, 0x06, 0x02,
    0x20, 0x01, 0xFF, 0x7F, 0xEB, 0x7E, 0x1F, 0x00, 0x00, 0x7C, 0xFF, 0x7F,
    0xFF, 0x3F, 0x00, 0x7E, 0x1F, 0x00, 0xFF, 0x7F, 0xFF, 0x03, 0x1F, 0x00,
    0x00, 0x00, 0xFF, 0x03, 0x1F, 0x00, 0x0C, 0x00, 0x00, 0x00, 0xFF, 0x7F,
    0x3F, 0x03, 0x93, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x7F, 0x03,
    0xFF, 0x7F, 0xFF, 0x7F, 0x8C, 0x7E, 0x00, 0x7C, 0x00, 0x00, 0xFF, 0x7F,
    0xEF, 0x1B, 0x80, 0x61, 0x00, 0x00, 0xFF, 0x7F, 0xEA, 0x7F, 0x5F, 0x7D,
    0x00, 0x00, 0x78, 0x47, 0x90, 0x32, 0x87, 0x1D, 0x61, 0x08, 0x03, 0x90,
    0x0F, 0x18, 0x00, 0x78, 0x81, 0x09, 0x12, 0x15, 0x54, 0x93, 0x99, 0x9C,
    0x9F, 0xA2, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C, 0x33, 0x00,
    0x03, 0x1C, 0x0F, 0x1F, 0x7B, 0x1C, 0x3E, 0x3C, 0xFD, 0xB6, 0xF7, 0xF7,
    0x71, 0x01, 0x7F, 0xFC, 0x30, 0xC0, 0x7F, 0x78, 0xFF, 0xFD, 0xDD, 0xCF,
    0x00, 0x23, 0x60, 0xFC, 0x6F, 0xD4, 0xBC, 0x35, 0x08, 0xFF, 0xC8, 0x80,
    0xF0, 0x53, 0xF8, 0x6A, 0xDF, 0x7C, 0x3D, 0x81, 0x7D, 0x79, 0x3C, 0xF3,
    0x43, 0xE7, 0x0F, 0xC7, 0x00, 0xFC, 0x01, 0xD2, 0xB4, 0xAD, 0x2B, 0x41,
    0x0E, 0x34, 0x13, 0x1C, 0x41, 0x38, 0x31, 0xFF, 0xEF, 0xF3, 0xE0, 0x5F,
    0xD7, 0xD7, 0xFF, 0x3F, 0xE0, 0xF6, 0xAF, 0xDA, 0x9F, 0xFD, 0xA9, 0xE8,
    0xFC, 0xDA, 0xBC, 0x3E, 0x7D, 0xA9, 0xE8, 0x00, 0xFF, 0xCF, 0x1F, 0xFF,
    0xFD, 0x28, 0x1D, 0x80, 0x1C, 0x3D, 0x3C, 0xFF, 0xF4, 0x2A, 0x38, 0xA9,
    0x3F, 0xFF, 0x40, 0x70, 0x00, 0xFF, 0xC5, 0xC0, 0xBF, 0xF6, 0xAA, 0xCF,
    0xE1, 0xD2, 0x00, 0xF3, 0xE3, 0xF7, 0xF3, 0xB4, 0x27, 0x77, 0x5F, 0xF5,
    0xFC, 0x38, 0x48, 0x00, 0xFF, 0xC7, 0x3F, 0xB4, 0xAD, 0x28, 0xEF, 0xAF,
    0xC4, 0xCF, 0x20, 0xCE, 0x8E, 0x9F, 0x90, 0x1E, 0xFF, 0xFF, 0x42, 0x1C,
    0xA9, 0x33, 0x00, 0xFF, 0x09, 0x9F, 0x8F, 0xE2, 0x1F, 0x5F, 0xFD, 0x48,
    0x3E, 0x3F, 0xA7, 0xBF, 0xCF, 0x3C, 0x42, 0x38, 0xA8, 0x7F, 0xAF, 0xFC,
    0x00, 0xFF, 0xCF, 0xFF, 0xFF, 0x3F, 0x00, 0xFF, 0xCA, 0xFE, 0xFF, 0xD4,
    0x00, 0xFF, 0xFF, 0x2B, 0xFE, 0xFD, 0x4F, 0x00, 0xFC, 0xFC, 0x53, 0xFF,
    0xFC, 0x0F, 0xF7, 0xFD, 0x30, 0xFF, 0x1E, 0x3D, 0xFC, 0x45, 0xFF, 0x87,
    0x1F, 0xF7, 0xBF, 0x03, 0xFF, 0x3F, 0xFE, 0x5D, 0x54, 0x00, 0xFF, 0xFC,
    0x01, 0xC0, 0x83, 0x03, 0x87, 0xD3, 0x4F, 0x54, 0x8F, 0x3C, 0xD2, 0xAA,
    0x2A, 0xFC, 0x06, 0xBE, 0x3E, 0xDF, 0xDF, 0xDD, 0xCF, 0x00, 0xC8, 0x0E,
    0xFF, 0x7B, 0xFC, 0xF3, 0x15, 0xC0, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF1,
    0x03, 0xC2, 0xFF, 0x87, 0xFF, 0x50, 0xF8, 0x00, 0xFF, 0x03, 0xC7, 0x83,
    0xE3, 0x61, 0xF1, 0xB6, 0x53, 0x7C, 0xDF, 0xDA, 0xAF, 0xF4, 0xA5, 0x4A,
    0xD7, 0xD7, 0x55, 0xD7, 0xFF, 0xF1, 0xE0, 0x6F, 0xDB, 0xF0, 0xF9, 0xF1,
    0x00, 0xFB, 0xF9, 0x7F, 0x7B, 0xB7, 0xFF, 0x3F, 0x1E, 0xD0, 0x1C, 0xAA,
    0xA4, 0xFF, 0xCF, 0x00, 0xFC, 0x3F, 0x13, 0x1E, 0x31, 0x7C, 0xF8, 0xE5,
    0xF5, 0xF5, 0xD7, 0xD7, 0x01, 0xFF, 0x7F, 0x4F, 0x77, 0xC7, 0x22, 0x9F,
    0x03, 0x7D, 0x01, 0x1D, 0x24, 0x38, 0x6D, 0x02, 0x71, 0xCD, 0xF4, 0x05,
    0x3E, 0x04, 0x0E, 0x00, 0xCB, 0x20, 0xF5, 0xCB, 0x11, 0xF1, 0xCB, 0x11,
    0x3D, 0x20, 0xF5, 0x79, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xE5, 0x21, 0x0F,
    0xFF, 0xCB, 0x86, 0xCB, 0x46, 0x28, 0xFC, 0xE1, 0xC9, 0xCD, 0x3F, 0x08,
    0xCD, 0x09, 0x06, 0x05, 0x20, 0xF7, 0xC9, 0xE0, 0x13, 0x3E, 0x87, 0xE0,
    0x14, 0xC9, 0x21, 0x00, 0x80, 0xAF, 0x22, 0xCB, 0x6C, 0x28, 0xFA, 0xC9,
    0xCD, 0x33, 0x06, 0x1A, 0xA1, 0x47, 0x1C, 0x1C, 0x1A, 0x1D, 0x1D, 0xA1,
    0xCB, 0x37, 0xB0, 0xCB, 0x41, 0x28, 0x02, 0xCB, 0x37, 0x23, 0x22, 0xCB,
    0x31, 0xC9, 0xCD, 0x4D, 0x06, 0xCD, 0x30, 0x06, 0x1C, 0x7B, 0xC9, 0x21,
    0x96, 0x04, 0x11, 0x7F, 0x80, 0x0E, 0x30, 0x46, 0x05, 0x28, 0x36, 0x04,
    0x23, 0x37, 0xCB, 0x10, 0x38, 0x20, 0xCB, 0x20, 0x38, 0x03, 0x2A, 0x18,
    0x20, 0xCB, 0x20, 0x20, 0x05, 0x46, 0x23, 0x37, 0xCB, 0x10, 0x4F, 0x30,
    0x03, 0xCB, 0x3F, 0xFE, 0x87, 0xCB, 0x20, 0x38, 0x02, 0xB1, 0xFE, 0xA1,
    0x18, 0x07, 0xCB, 0x20, 0x38, 0x03, 0x1B, 0x1A, 0x13, 0x13, 0x12, 0xCB,
    0x20, 0x20, 0xD1, 0x18, 0xC6, 0x62, 0x2E, 0x80, 0x11, 0x04, 0x01, 0x0E,
    0xF0, 0xCD, 0x4A, 0x06, 0xC6, 0x16, 0x5F, 0xCD, 0x4A, 0x06, 0xD6, 0x16,
    0x5F, 0xFE, 0x1C, 0x20, 0xEE, 0x23, 0x11, 0x8E, 0x04, 0x0E, 0x08, 0x1A,
    0x13, 0x22, 0x23, 0x0D, 0x20, 0xF9, 0xC9, 0x3E, 0x01, 0xE0, 0x4F, 0x16,
    0x1A, 0x06, 0x02, 0xCD, 0x15, 0x06, 0x21, 0xC0, 0x98, 0x0E, 0x03, 0x7E,
    0xFE, 0x0F, 0x28, 0x05, 0x34, 0xE6, 0x07, 0x28, 0x03, 0x23, 0x18, 0xF3,
    0x7D, 0xF6, 0x1F, 0x6F, 0x23, 0x0D, 0x20, 0xEB, 0x15, 0x20, 0xDE, 0xC9,
    0x06, 0x20, 0x0E, 0x20, 0x21, 0x81, 0xFF, 0xC5, 0x2A, 0x5F, 0x3A, 0x57,
    0x01, 0x21, 0x04, 0x7B, 0xE6, 0x1F, 0xFE, 0x1F, 0x20, 0x01, 0x0D, 0x7B,
    0xFE, 0xE0, 0x38, 0x09, 0x7A, 0xE6, 0x03, 0xFE, 0x03, 0x20, 0x02, 0xCB,
    0xA9, 0x7A, 0xE6, 0x7C, 0xFE, 0x7C, 0x20, 0x02, 0xCB, 0x90, 0x7B, 0x81,
    0x22, 0x7A, 0x88, 0x22, 0xC1, 0x0D, 0x20, 0xCF, 0xCD, 0x09, 0x06, 0xCD,
    0x18, 0x08, 0xCD, 0x09, 0x06, 0x05, 0x20, 0xBE, 0x3E, 0x02, 0xE0, 0x70,
    0x21, 0x00, 0xD0, 0xCD, 0x29, 0x06, 0x3C, 0xCD, 0x2C, 0x08, 0xCD, 0x31,
    0x08, 0xCD, 0x2C, 0x08, 0xAF, 0xE0, 0x70, 0x2F, 0xE0, 0x00, 0x57, 0x59,
    0x2E, 0x0D, 0xFA, 0x43, 0x01, 0xCB, 0x7F, 0xCC, 0x73, 0x07, 0xCB, 0x7F,
    0xE0, 0x4C, 0xF0, 0x80, 0x47, 0x28, 0x05, 0xF0, 0xC1, 0xA7, 0x20, 0x06,
    0xAF, 0x4F, 0x3E, 0x11, 0x61, 0xC9, 0xCD, 0x73, 0x07, 0xE0, 0x4C, 0x3E,
    0x01, 0xC9, 0x21, 0x7D, 0x04, 0x4F, 0x06, 0x00, 0x09, 0x7E, 0xC9, 0x3E,
    0x01, 0xE0, 0x6C, 0xCD, 0x9E, 0x07, 0xCB, 0x7F, 0xC4, 0xDF, 0x08, 0xCB,
    0xBF, 0x47, 0x80, 0x80, 0x47, 0xF0, 0xC1, 0xA7, 0x28, 0x05, 0xCD, 0x6A,
    0x07, 0x18, 0x01, 0x78, 0xCD, 0x09, 0x06, 0xCD, 0xF3, 0x07, 0x3E, 0x04,
    0x11, 0x08, 0x00, 0x2E, 0x7C, 0xC9, 0x21, 0x4B, 0x01, 0x7E, 0xFE, 0x33,
    0x28, 0x05, 0x3D, 0x20, 0x40, 0x18, 0x0C, 0x2E, 0x44, 0x2A, 0xFE, 0x30,
    0x20, 0x37, 0x7E, 0xFE, 0x31, 0x20, 0x32, 0x2E, 0x34, 0x0E, 0x10, 0xAF,
    0x86, 0x2C, 0x0D, 0x20, 0xFB, 0x47, 0x21, 0x00, 0x02, 0x7D, 0xD6, 0x5E,
    0xC8, 0x2A, 0xB8, 0x20, 0xF8, 0x7D, 0xD6, 0x42, 0x38, 0x0E, 0xE5, 0x7D,
    0xC6, 0x7A, 0x6F, 0x7E, 0xE1, 0x4F, 0xFA, 0x37, 0x01, 0xB9, 0x20, 0xE5,
    0x7D, 0xC6, 0x5D, 0x6F, 0x78, 0xE0, 0x80, 0x7E, 0xC9, 0xAF, 0xC9, 0x21,
    0xD9, 0x02, 0x06, 0x00, 0x4F, 0x09, 0xC9, 0xCD, 0xEB, 0x07, 0x1E, 0x00,
    0x2A, 0xE5, 0x21, 0x7E, 0x03, 0x4F, 0x09, 0x16, 0x08, 0x0E, 0x6A, 0xCD,
    0x21, 0x08, 0xE1, 0xCB, 0x5B, 0x20, 0x04, 0x1E, 0x08, 0x18, 0xE9, 0x4E,
    0x21, 0x7E, 0x03, 0x09, 0x16, 0x08, 0x18, 0x05, 0x21, 0x81, 0xFF, 0x16,
    0x40, 0x1E, 0x00, 0x0E, 0x68, 0x3E, 0x80, 0xB3, 0xE2, 0x0C, 0x2A, 0xE2,
    0x15, 0x20, 0xFB, 0xC9, 0xE0, 0x4F, 0x21, 0xEE, 0x00, 0xCD, 0x09, 0x06,
    0x0E, 0x51, 0x06, 0x05, 0x2A, 0xE2, 0x0C, 0x05, 0x20, 0xFA, 0xC9, 0x3E,
    0x20, 0xE0, 0x00, 0xF0, 0x00, 0x2F, 0xE6, 0x0F, 0xC8, 0x2E, 0x00, 0x2C,
    0x1F, 0x30, 0xFC, 0x3E, 0x10, 0xE0, 0x00, 0xF0, 0x00, 0x2F, 0x17, 0x17,
    0xE6, 0x0C, 0x85, 0x6F, 0xF0, 0xC1, 0xBD, 0xC8, 0x7D, 0xE0, 0xC1, 0xC5,
    0xD5, 0xCD, 0x6A, 0x07, 0xCD, 0xEB, 0x07, 0x2C, 0x2C, 0x4E, 0x21, 0x7F,
    0x03, 0x09, 0x3A, 0xFE, 0x7F, 0x20, 0x02, 0x23, 0x23, 0xF5, 0x2A, 0xE5,
    0x21, 0x81, 0xFF, 0xCD, 0xD5, 0x08, 0x2E, 0x83, 0xCD, 0xD5, 0x08, 0xE1,
    0xE0, 0x87, 0x2A, 0xE5, 0x21, 0x82, 0xFF, 0xCD, 0xD5, 0x08, 0x2E, 0x84,
    0xCD, 0xD5, 0x08, 0xE1, 0xE0, 0x88, 0xF1, 0x28, 0x02, 0x23, 0x23, 0xF0,
    0xBB, 0xE6, 0xDE, 0x47, 0x2A, 0xE6, 0xDE, 0x80, 0x47, 0xFA, 0xBC, 0xFF,
    0xCB, 0x97, 0x4E, 0xCB, 0x91, 0x89, 0x1F, 0xEA, 0xBC, 0xFF, 0x78, 0x1F,
    0xEA, 0xBB, 0xFF, 0x2D, 0x2A, 0xE0, 0xBF, 0x2A, 0xE0, 0xC0, 0x2A, 0xE0,
    0x85, 0x2A, 0xE0, 0x86, 0xCD, 0x09, 0x06, 0xCD, 0x18, 0x08, 0x3E, 0x30,
    0xE0, 0xC2, 0xD1, 0xC1, 0xC9, 0x11, 0x08, 0x00, 0x4B, 0x77, 0x19, 0x0D,
    0x20, 0xFB, 0xC9, 0xF5, 0xCD, 0x09, 0x06, 0x3E, 0x19, 0xEA, 0x10, 0x99,
    0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20, 0xF9,
    0x2E, 0x0F, 0x18, 0xF5, 0xF1, 0xC9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
