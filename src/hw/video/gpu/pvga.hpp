// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;

class pvga {
public:
	pvga(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	constexpr const char *get_name() { return "NV2A.PVGA"; }
	uint8_t io_read8(uint32_t addr);
	void io_write8(uint32_t addr, const uint8_t data);
	void io_write16(uint32_t addr, const uint16_t data);
	uint8_t mem_read8(uint32_t addr);
	uint16_t mem_read16(uint32_t addr);
	void mem_write8(uint32_t addr, const uint8_t data);
	void mem_write16(uint32_t addr, const uint16_t data);

private:
	machine *const m_machine;
};
