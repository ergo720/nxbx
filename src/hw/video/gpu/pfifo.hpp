// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PFIFO 0x00002000
#define NV_PFIFO_BASE (NV2A_REGISTER_BASE + NV_PFIFO)
#define NV_PFIFO_SIZE 0x2000

#define NV_PFIFO_RAMHT (NV2A_REGISTER_BASE + 0x00002210) // Contains the base address and size of ramht in ramin
#define NV_PFIFO_RAMFC (NV2A_REGISTER_BASE + 0x00002214) // Contains the base address and size of ramfc in ramin
#define NV_PFIFO_RAMRO (NV2A_REGISTER_BASE + 0x00002218) // Contains the base address and size of ramro in ramin


class machine;

class pfifo {
public:
	pfifo(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false, bool enabled = true>
	uint32_t read(uint32_t addr);
	template<bool log = false, bool enabled = true>
	void write(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool enabled, bool is_be);

	machine *const m_machine;
	struct {
		uint32_t ramht, ramfc, ramro;
	};
};
