// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PVIDEO 0x00008000
#define NV_PVIDEO_MMIO_BASE (NV2A_REGISTER_BASE + NV_PVIDEO)
#define NV_PVIDEO_SIZE 0x1000

#define NV_PVIDEO_DEBUG_0 (NV2A_REGISTER_BASE + 0x00008080) // debug flags 0
#define NV_PVIDEO_DEBUG_1 (NV2A_REGISTER_BASE + 0x00008084) // debug flags 1
#define NV_PVIDEO_DEBUG_2 (NV2A_REGISTER_BASE + 0x00008088) // debug flags 2
#define NV_PVIDEO_DEBUG_3 (NV2A_REGISTER_BASE + 0x0000808C) // debug flags 3
#define NV_PVIDEO_DEBUG_4 (NV2A_REGISTER_BASE + 0x00008090) // debug flags 4
#define NV_PVIDEO_DEBUG_5 (NV2A_REGISTER_BASE + 0x00008094) // debug flags 5
#define NV_PVIDEO_DEBUG_6 (NV2A_REGISTER_BASE + 0x00008098) // debug flags 6
#define NV_PVIDEO_DEBUG_7 (NV2A_REGISTER_BASE + 0x0000809C) // debug flags 7
#define NV_PVIDEO_DEBUG_8 (NV2A_REGISTER_BASE + 0x000080A0) // debug flags 8
#define NV_PVIDEO_DEBUG_9 (NV2A_REGISTER_BASE + 0x000080A4) // debug flags 9
#define NV_PVIDEO_DEBUG_10 (NV2A_REGISTER_BASE + 0x000080A8) // debug flags 10
#define NV_PVIDEO_INTR (NV2A_REGISTER_BASE + 0x00008100) // Pending pvideo interrupts. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PVIDEO_INTR_EN (NV2A_REGISTER_BASE + 0x00008140) // Enable/disable pvideo interrupts
#define NV_PVIDEO_BASE(i) (NV2A_REGISTER_BASE + 0x00008900 + (i) * 4) // TODO
#define NV_PVIDEO_LUMINANCE(i) (NV2A_REGISTER_BASE + 0x00008910 + (i) * 4) // Unknown
#define NV_PVIDEO_CHROMINANCE(i) (NV2A_REGISTER_BASE + 0x00008918 + (i) * 4) // Unknown
#define NV_PVIDEO_SIZE_IN(i) (NV2A_REGISTER_BASE + 0x00008928 + (i) * 4) // TODO
#define NV_PVIDEO_POINT_IN(i) (NV2A_REGISTER_BASE + 0x00008930 + (i) * 4) // TODO
#define NV_PVIDEO_DS_DX(i) (NV2A_REGISTER_BASE + 0x00008938 + (i) * 4) // TODO
#define NV_PVIDEO_DT_DY(i) (NV2A_REGISTER_BASE + 0x00008940 + (i) * 4) // TODO


class cpu;
class nv2a;

class pvideo
{
public:
	pvideo();
	~pvideo();
	void init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo();
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
