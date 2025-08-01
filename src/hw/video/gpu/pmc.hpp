// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PMC 0x00000000
#define NV_PMC_BASE (NV2A_REGISTER_BASE + NV_PMC)
#define NV_PMC_SIZE 0x1000

#define NV_PMC_BOOT_0 (NV2A_REGISTER_BASE + 0x00000000) // Contains the gpu identification number
#define NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0 0x02A000A3
#define NV_PMC_BOOT_1 (NV2A_REGISTER_BASE + 0x00000004) // Switches the endianness of all accesses done through BAR0. PVGA is not affected because all its registers are single bytes
#define NV_PMC_BOOT_1_ENDIAN0_LITTLE (0x00000000 << 0)
#define NV_PMC_BOOT_1_ENDIAN0_BIG (0x00000001 << 0)
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE (0x00000000 << 24)
#define NV_PMC_BOOT_1_ENDIAN24_BIG (0x00000001 << 24)
#define NV_PMC_INTR_0 (NV2A_REGISTER_BASE + 0x00000100) // Pending interrupts of all engines
#define NV_PMC_INTR_0_PFIFO 8
#define NV_PMC_INTR_0_PGRAPH 12
#define NV_PMC_INTR_0_PTIMER 20
#define NV_PMC_INTR_0_PCRTC 24
#define NV_PMC_INTR_0_SOFTWARE 31
#define NV_PMC_INTR_0_NOT_PENDING 0x00000000
#define NV_PMC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00000140) // Enable/disable hw/sw interrupts
#define NV_PMC_INTR_EN_0_INTA_DISABLED 0x00000000
#define NV_PMC_INTR_EN_0_INTA_HARDWARE 0x00000001
#define NV_PMC_INTR_EN_0_INTA_SOFTWARE 0x00000002
#define NV_PMC_ENABLE (NV2A_REGISTER_BASE + 0x00000200) // Enable/disable gpu engines
#define NV_PMC_ENABLE_PFIFO (1 << 8)
#define NV_PMC_ENABLE_PGRAPH (1 << 12)
#define NV_PMC_ENABLE_PTIMER (1 << 16)
#define NV_PMC_ENABLE_PFB (1 << 20)
#define NV_PMC_ENABLE_PCRTC (1 << 24)
#define NV_PMC_ENABLE_PVIDEO (1 << 28)
#define NV_PMC_ENABLE_ALL (NV_PMC_ENABLE_PFIFO | NV_PMC_ENABLE_PGRAPH | NV_PMC_ENABLE_PTIMER | NV_PMC_ENABLE_PFB | NV_PMC_ENABLE_PCRTC | NV_PMC_ENABLE_PVIDEO)


class machine;
class pfifo;
class ptimer;
class pfb;
class pcrtc;
class pvideo;
class pbus;
class pramdac;
class pramin;
class puser;
class pgraph;

class pmc {
public:
	pmc(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	void update_irq();
	template<bool log = false>
	uint32_t read32(uint32_t addr);
	template<bool log = false>
	void write32(uint32_t addr, const uint32_t value);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool is_be);

	friend class pfifo;
	friend class ptimer;
	friend class pfb;
	friend class pcrtc;
	friend class pvideo;
	friend class pbus;
	friend class pramdac;
	friend class pramin;
	friend class puser;
	friend class pgraph;
	machine *const m_machine;
	// registers
	uint32_t endianness;
	uint32_t int_status;
	uint32_t int_enabled;
	uint32_t engine_enabled;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PMC_BOOT_0, "NV_PMC_BOOT_0" },
		{ NV_PMC_BOOT_1, "NV_PMC_BOOT_1" },
		{ NV_PMC_INTR_0, "NV_PMC_INTR_0" },
		{ NV_PMC_INTR_0, "NV_PMC_INTR_0" },
		{ NV_PMC_INTR_EN_0, "NV_PMC_INTR_EN_0" },
		{ NV_PMC_ENABLE, "NV_PMC_ENABLE" }
	};
};
