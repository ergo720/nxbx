// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <chrono>


class machine;

class cmos {
public:
	cmos(machine *machine) : m_machine(machine) {}
	bool init();
	void deinit();
	void reset();
	void update_io_logging() { update_io(true); }
	uint8_t read_handler(uint32_t addr);
	void write_handler(uint32_t addr, const uint8_t data);
	uint8_t read_handler_logger(uint32_t addr);
	void write_handler_logger(uint32_t addr, const uint8_t data);
	uint64_t get_next_update_time(uint64_t now);

private:
	bool update_io(bool is_update);
	void update_time(uint64_t elapsed_us);
	uint8_t to_bcd(uint8_t data);
	uint8_t from_bcd(uint8_t data);

	machine *const m_machine;
	uint8_t ram[128]; // byte at index 0x7F is the century register on the xbox
	uint8_t reg_idx;
	uint64_t last_update_time;
	uint64_t lost_us;
	std::time_t sys_time;
	int64_t sys_time_bias; // difference between guest and host clocks
};
