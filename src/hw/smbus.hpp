// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <stdint.h>


class machine;

class smbus {
public:
	smbus(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t data);

private:
	bool update_io(bool is_update);
	void start_cycle();

	machine *const m_machine;
	uint8_t m_regs[5];
};
