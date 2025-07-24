// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <unordered_map>
#include <string>

#define NV2A_CLOCK_FREQ 233333324 // = 233 MHz
#define NV2A_CRYSTAL_FREQ 16666666 // = 16 MHz
#define NV2A_IRQ_NUM 3
#define NV2A_REGISTER_BASE 0xFD000000
#define NV2A_VRAM_BASE 0xF0000000
#define NV2A_VRAM_SIZE64 0x4000000 // = 64 MiB
#define NV2A_VRAM_SIZE128 0x8000000 // = 128 MiB
#define NV2A_MAX_NUM_CHANNELS 32 // max num of fifo queues

// DMA object masks
#define NV_DMA_CLASS 0x00000FFF
#define NV_DMA_ADJUST 0xFFF00000
#define NV_DMA_ADDRESS 0xFFFFF000
#define NV_DMA_TARGET 0x0030000
