// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PFB 0x00100000
#define NV_PFB_BASE (NV2A_REGISTER_BASE + NV_PFB)
#define NV_PFB_SIZE 0x1000
#define REGS_PFB_idx(x) ((x - NV_PFB_BASE) >> 2)

#define NV_PFB_CFG0 (NV2A_REGISTER_BASE + 0x00100200) // Appear to contain info about the ram modules
#define NV_PFB_CFG1 (NV2A_REGISTER_BASE + 0x00100204) // Appear to contain info about the ram modules
#define NV_PFB_CSTATUS (NV2A_REGISTER_BASE + 0x0010020C) // Returns the size of the framebuffer in MiB in the bits 20-31. Bit 0 is a flag that indicates > 4 GiB of fb when set
#define NV_PFB_NVM (NV2A_REGISTER_BASE + 0x00100214) // FIXME: unknown what this does
#define NV_PFB_WBC (NV2A_REGISTER_BASE + 0x00100410) // related to the write combine cache
#define NV_PFB_WBC_FLUSH (1 << 16) // pending cache flush=1


class machine;
class pramin;

class pfb {
public:
	pfb(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false, bool enabled = true>
	uint32_t read32(uint32_t addr);
	template<bool log = false, bool enabled = true>
	void write32(uint32_t addr, const uint32_t value);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool enabled, bool is_be);

	friend class pramin;
	machine *const m_machine;
	// registers
	uint32_t m_regs[NV_PFB_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PFB_CFG0, "NV_PFB_CFG0" },
		{ NV_PFB_CFG1, "NV_PFB_CFG1" },
		{ NV_PFB_CSTATUS, "NV_PFB_CSTATUS" },
		{ NV_PFB_NVM, "NV_PFB_NVM" },
		{ NV_PFB_WBC, "NV_PFB_WBC" },
	};
};
