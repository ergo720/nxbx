// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PVIDEO 0x00008000
#define NV_PVIDEO_MMIO_BASE (NV2A_REGISTER_BASE + NV_PVIDEO)
#define NV_PVIDEO_SIZE 0x1000

#define NV_PVIDEO_DEBUG_0 (NV2A_REGISTER_BASE + 0x00008080) // Unknown
#define NV_PVIDEO_DEBUG_1 (NV2A_REGISTER_BASE + 0x00008084) // Unknown
#define NV_PVIDEO_DEBUG_2 (NV2A_REGISTER_BASE + 0x00008088) // Unknown
#define NV_PVIDEO_DEBUG_3 (NV2A_REGISTER_BASE + 0x0000808C) // Unknown
#define NV_PVIDEO_DEBUG_4 (NV2A_REGISTER_BASE + 0x00008090) // Unknown
#define NV_PVIDEO_DEBUG_5 (NV2A_REGISTER_BASE + 0x00008094) // Unknown
#define NV_PVIDEO_DEBUG_6 (NV2A_REGISTER_BASE + 0x00008098) // Unknown
#define NV_PVIDEO_DEBUG_7 (NV2A_REGISTER_BASE + 0x0000809C) // Unknown
#define NV_PVIDEO_DEBUG_8 (NV2A_REGISTER_BASE + 0x000080A0) // Unknown
#define NV_PVIDEO_DEBUG_9 (NV2A_REGISTER_BASE + 0x000080A4) // Unknown
#define NV_PVIDEO_DEBUG_10 (NV2A_REGISTER_BASE + 0x000080A8) // Unknown
#define NV_PVIDEO_BASE(i) (NV2A_REGISTER_BASE + 0x00008900 + (i) * 4) // TODO
#define NV_PVIDEO_LUMINANCE(i) (NV2A_REGISTER_BASE + 0x00008910 + (i) * 4) // Unknown
#define NV_PVIDEO_CHROMINANCE(i) (NV2A_REGISTER_BASE + 0x00008918 + (i) * 4) // Unknown
#define NV_PVIDEO_SIZE_IN(i) (NV2A_REGISTER_BASE + 0x00008928 + (i) * 4) // TODO
#define NV_PVIDEO_POINT_IN(i) (NV2A_REGISTER_BASE + 0x00008930 + (i) * 4) // TODO
#define NV_PVIDEO_DS_DX(i) (NV2A_REGISTER_BASE + 0x00008938 + (i) * 4) // TODO
#define NV_PVIDEO_DT_DY(i) (NV2A_REGISTER_BASE + 0x00008940 + (i) * 4) // TODO


class machine;

class pvideo {
public:
	pvideo(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false, bool enabled = true>
	uint32_t read32(uint32_t addr);
	template<bool log = false, bool enabled = true>
	void write32(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool enabled, bool is_be);

	machine *const m_machine;
	// registers
	uint32_t debug[11];
	uint32_t regs[24];
};
