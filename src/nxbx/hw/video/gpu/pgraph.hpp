// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2025 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PGRAPH 0x00400000
#define NV_PGRAPH_BASE (NV2A_REGISTER_BASE + NV_PGRAPH)
#define NV_PGRAPH_SIZE 0x2000
#define REGS_PGRAPH_idx(x) ((x - NV_PGRAPH_BASE) >> 2)
#define REG_PGRAPH(r) (m_regs[REGS_PGRAPH_idx(r)])

#define NV_PGRAPH_INTR (NV2A_REGISTER_BASE + 0x00400100) // Pending pgraph interrupts. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PGRAPH_INTR_EN (NV2A_REGISTER_BASE + 0x00400140) // Enable/disable pgraph interrupts


class machine;
class pmc;
enum engine_enabled : int;

class pgraph {
public:
	pgraph(machine *machine) : m_machine(machine) {}
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
	uint32_t m_regs[NV_PGRAPH_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PGRAPH_INTR, "NV_PGRAPH_INTR" },
		{ NV_PGRAPH_INTR_EN, "NV_PGRAPH_INTR_EN" },
	};
};
