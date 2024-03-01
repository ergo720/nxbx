// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;
class pmc;
class pcrtc;
class pramdac;
class pfb;
class pbus;

class ptimer {
public:
	ptimer(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	constexpr const char *get_name() { return "NV2A.PTIMER"; }
	uint64_t get_next_alarm_time(uint64_t now);
	uint32_t read(uint32_t addr);
	void write(uint32_t addr, const uint32_t data);

private:
	friend class pmc;
	friend class pcrtc;
	friend class pramdac;
	friend class pfb;
	friend class pbus;
	uint64_t counter_to_us();

	machine *const m_machine;
	// Host time when the last alarm interrupt was triggered
	uint64_t last_alarm_time;
	// Time in us before the alarm triggers
	uint64_t counter_period;
	// Counter is running if 1
	uint8_t counter_active;
	// offset added to counter
	uint64_t counter_offset;
	struct {
		// Pending alarm interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
		uint32_t int_status;
		// Enable/disable alarm interrupt
		uint32_t int_enabled;
		// Multiplier and divider form a ratio which is then used to multiply the clock frequency
		uint32_t multiplier, divider;
		// Counter value that triggers the alarm interrupt
		uint32_t alarm;
	};
};
