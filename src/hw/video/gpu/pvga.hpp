// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"


class machine;

class pvga {
public:
	pvga(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false>
	uint8_t io_read8(uint32_t addr);
	template<bool log = false>
	void io_write8(uint32_t addr, const uint8_t data);
	template<bool log = false>
	void io_write16(uint32_t addr, const uint16_t data);
	template<bool log = false>
	uint8_t mem_read8(uint32_t addr);
	template<bool log = false>
	uint16_t mem_read16(uint32_t addr);
	template<bool log = false>
	void mem_write8(uint32_t addr, const uint8_t data);
	template<bool log = false>
	void mem_write16(uint32_t addr, const uint16_t data);

private:
	bool update_io(bool is_update);
	machine *const m_machine;
};
