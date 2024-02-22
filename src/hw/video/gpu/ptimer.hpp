// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


struct ptimer_t {
	// Pending alarm interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
	uint32_t int_status;
	// Enable/disable alarm interrupt
	uint32_t int_enabled;
	// Multiplier and divider form a ratio which is then used to multiply the clock frequency
	uint32_t multiplier, divider;
	// Counter value that triggers the alarm interrupt
	uint32_t alarm;
	// Host time when the last alarm interrupt was triggered
	uint64_t last_alarm_time;
	// Time in us before the alarm triggers
	uint64_t counter_period;
	// Counter is running if 1
	uint8_t counter_active;
	// offset added to counter
	uint64_t counter_offset;
};

void ptimer_init();
uint64_t ptimer_get_next_alarm_time(uint64_t now);
