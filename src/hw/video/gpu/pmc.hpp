// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PMC_BOOT_1 (NV2A_REGISTER_BASE + 0x00000004)
#define NV_PMC_BOOT_1_ENDIAN00_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN00_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN24_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK (0x00000000 << 0)
#define NV_PMC_BOOT_1_ENDIAN0_BIG_MASK (0x00000001 << 0)
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE_MASK (0x00000000 << 24)
#define NV_PMC_BOOT_1_ENDIAN24_BIG_MASK (0x00000001 << 24)
#define NV_PMC_ENABLE (NV2A_REGISTER_BASE + 0x00000200)
#define NV_PMC_ENABLE_PFIFO (1 << 8)
#define NV_PMC_ENABLE_PTIMER (1 << 16)
#define NV_PMC_ENABLE_PFB (1 << 20)
#define NV_PMC_ENABLE_PCRTC (1 << 24)
#define NV_PMC_ENABLE_PVIDEO (1 << 28)
#define NV_PMC_ENABLE_MASK (NV_PMC_ENABLE_PFIFO | NV_PMC_ENABLE_PTIMER | NV_PMC_ENABLE_PFB | NV_PMC_ENABLE_PCRTC | NV_PMC_ENABLE_PVIDEO)


class machine;
class pfifo;
class ptimer;
class pfb;
class pcrtc;
class pvideo;
class pbus;
class pramdac;
class pramin;

class pmc {
public:
	pmc(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	void update_irq();
	template<bool log = false>
	uint32_t read(uint32_t addr);
	template<bool log = false>
	void write(uint32_t addr, const uint32_t data);

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
	machine *const m_machine;
	struct {
		// This register switches the endianness of all accesses done through BAR0 and BAR2/3 (when present). PVGA is not affected
		// because all its register are single bytes
		uint32_t endianness;
		// Pending interrupts of all engines
		uint32_t int_status;
		// Enable/disable hw/sw interrupts
		uint32_t int_enabled;
		// Enable/disable gpu engines
		uint32_t engine_enabled;
	};
};
