// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;

class pmc {
public:
	pmc(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	void update_irq();
	template<bool log = false>
	uint32_t read(uint32_t addr);
	template<bool log = false>
	void write(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);

	machine *const m_machine;
	struct {
		uint32_t endianness;
		// Pending interrupts of all engines
		uint32_t int_status;
		// Enable/disable hw/sw interrupts
		uint32_t int_enabled;
		// Enable/disable gpu engines
		uint32_t engine_enabled;
	};
};
