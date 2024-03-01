// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;
class pcrtc;
class pramdac;
class ptimer;
class pfb;
class pbus;

class pmc {
public:
	pmc(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	constexpr const char *get_name() { return "NV2A.PMC"; }
	void update_irq();
	uint32_t read(uint32_t addr);
	void write(uint32_t addr, const uint32_t data);

private:
	friend class pcrtc;
	friend class pramdac;
	friend class ptimer;
	friend class pfb;
	friend class pbus;
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
