// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PTIMER 0x00009000
#define NV_PTIMER_BASE (NV2A_REGISTER_BASE + NV_PTIMER)
#define NV_PTIMER_SIZE 0x1000

#define NV_PTIMER_INTR_0 (NV2A_REGISTER_BASE + 0x00009100) // Pending alarm interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PTIMER_INTR_0_ALARM_NOT_PENDING 0x00000000
#define NV_PTIMER_INTR_0_ALARM_PENDING 0x00000001
#define NV_PTIMER_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00009140) // Enable/disable alarm interrupt
#define NV_PTIMER_INTR_EN_0_ALARM_DISABLED 0x00000000
#define NV_PTIMER_INTR_EN_0_ALARM_ENABLED 0x00000001
#define NV_PTIMER_NUMERATOR (NV2A_REGISTER_BASE + 0x00009200) // Divider forms a ratio which is then used to multiply the clock frequency
#define NV_PTIMER_NUMERATOR_MASK 0xFFFF
#define NV_PTIMER_DENOMINATOR (NV2A_REGISTER_BASE + 0x00009210) // Multiplier forms a ratio which is then used to multiply the clock frequency
#define NV_PTIMER_DENOMINATOR_MASK 0xFFFF
#define NV_PTIMER_TIME_0 (NV2A_REGISTER_BASE + 0x00009400) // Current gpu time (low bits)
#define NV_PTIMER_TIME_1 (NV2A_REGISTER_BASE + 0x00009410) // Current gpu time (high bits)
#define NV_PTIMER_ALARM_0 (NV2A_REGISTER_BASE + 0x00009420) // Counter value that triggers the alarm interrupt


class machine;
class pmc;
class pramdac;
enum engine_enabled : int;

class ptimer {
public:
	ptimer(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	uint64_t get_next_alarm_time(uint64_t now);
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool enabled, bool is_be);
	uint64_t counter_to_us();

	friend class pmc;
	friend class pramdac;
	machine *const m_machine;
	// Host time when the last alarm interrupt was triggered
	uint64_t last_alarm_time;
	// Time in us before the alarm triggers
	uint64_t counter_period;
	// Bias added/subtracted to counter before an alarm is due
	int64_t counter_bias;
	// Counter is running if not zero
	uint8_t counter_active;
	// Offset added to counter
	uint64_t counter_offset;
	// Counter value when it was stopped
	uint64_t counter_when_stopped;
	// registers
	uint32_t int_status;
	uint32_t int_enabled;
	uint32_t multiplier, divider;
	uint32_t alarm;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PTIMER_INTR_0, "NV_PTIMER_INTR_0" },
		{ NV_PTIMER_INTR_EN_0, "NV_PTIMER_INTR_EN_0" },
		{ NV_PTIMER_NUMERATOR, "NV_PTIMER_NUMERATOR" },
		{ NV_PTIMER_DENOMINATOR, "NV_PTIMER_DENOMINATOR" },
		{ NV_PTIMER_TIME_0, "NV_PTIMER_TIME_0" },
		{ NV_PTIMER_TIME_1, "NV_PTIMER_TIME_1" },
		{ NV_PTIMER_ALARM_0, "NV_PTIMER_ALARM_0" }
	};
};
