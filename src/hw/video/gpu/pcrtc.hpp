// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;
class pmc;
class pramdac;
class ptimer;
class pfb;
class pbus;
class pramin;

class pcrtc {
public:
	pcrtc(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	constexpr const char *get_name() { return "NV2A.PCRTC"; }
	uint32_t read(uint32_t addr);
	void write(uint32_t addr, const uint32_t data);

private:
	friend class pmc;
	friend class pramdac;
	friend class ptimer;
	friend class pfb;
	friend class pbus;
	friend class pramin;
	machine *const m_machine;
	struct {
		// Pending vblank interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
		uint32_t int_status;
		// Enable/disable vblank interrupt
		uint32_t int_enabled;
	};
};
