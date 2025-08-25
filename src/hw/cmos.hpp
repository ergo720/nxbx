// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <chrono>

#define CMOS_PORT_CMD 0x70
#define CMOS_PORT_DATA 0x71


class machine;

class cmos {
public:
	cmos(machine *machine) : m_machine(machine) {}
	bool init();
	void deinit();
	void reset();
	void update_io_logging() { update_io(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);
	uint64_t get_next_update_time(uint64_t now);

private:
	bool update_io(bool is_update);
	void update_clock(uint64_t elapsed_us);
	uint64_t update_periodic_ticks(uint64_t elapsed_us);
	void update_timer();
	void raise_irq(uint8_t why);
	uint8_t to_bcd(uint8_t value);
	uint8_t from_bcd(uint8_t value);
	uint8_t read(uint8_t idx);

	machine *const m_machine;
	uint8_t m_ram[128 * 2]; // byte at index 0x7F is the century register on the xbox
	uint8_t m_reg_idx;
	uint8_t m_int_running;
	uint8_t m_clock_running;
	uint64_t m_period_int; // expressed in us
	uint64_t m_periodic_ticks;
	uint64_t m_periodic_ticks_max;
	uint64_t m_last_int; // The last time the timer handler was called
	uint64_t m_last_clock; // The last time the seconds counter rolled over
	uint64_t m_lost_ticks; // expressed in us
	uint64_t m_lost_us;
	std::time_t m_sys_time; // actual real time wall clock of the host
	int64_t m_sys_time_bias; // difference between guest and host clocks
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ CMOS_PORT_CMD, "COMMAND" },
		{ CMOS_PORT_DATA, "DATA" },
	};
};
