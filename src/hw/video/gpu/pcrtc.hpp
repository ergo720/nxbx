// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PCRTC 0x00600000
#define NV_PCRTC_BASE (NV2A_REGISTER_BASE + NV_PCRTC)
#define NV_PCRTC_SIZE 0x1000

#define NV_PCRTC_INTR_0 (NV2A_REGISTER_BASE + 0x00600100) // Pending vblank interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PCRTC_INTR_0_VBLANK_NOT_PENDING 0x00000000
#define NV_PCRTC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00600140) // Enable/disable vblank interrupt
#define NV_PCRTC_INTR_EN_0_VBLANK_DISABLED 0x00000000
#define NV_PCRTC_START (NV2A_REGISTER_BASE + 0x00600800) // The address of the framebuffer
#define NV_PCRTC_UNKNOWN0 (NV2A_REGISTER_BASE + 0x00600804) // Unknown


class machine;
class pmc;
enum engine_enabled : int;

class pcrtc {
public:
	pcrtc(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool enabled, bool is_be);

	friend class pmc;
	machine *const m_machine;
	// registers
	uint32_t int_status;
	uint32_t int_enabled;
	uint32_t fb_addr;
	uint32_t unknown[1];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PCRTC_INTR_0, "NV_PCRTC_INTR_0" },
		{ NV_PCRTC_INTR_EN_0, "NV_PCRTC_INTR_EN_0" },
		{ NV_PCRTC_START, "NV_PCRTC_START" },
		{ NV_PCRTC_UNKNOWN0, "NV_PCRTC_UNKNOWN0" },
	};
};
