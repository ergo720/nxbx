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
	void update_time(uint64_t elapsed_us);
	uint8_t to_bcd(uint8_t value);
	uint8_t from_bcd(uint8_t value);

	machine *const m_machine;
	uint8_t m_ram[128]; // byte at index 0x7F is the century register on the xbox
	uint8_t m_reg_idx;
	uint64_t m_last_update_time;
	uint64_t m_lost_us;
	std::time_t m_sys_time;
	int64_t m_sys_time_bias; // difference between guest and host clocks
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ CMOS_PORT_CMD, "COMMAND" },
		{ CMOS_PORT_DATA, "DATA" },
	};
};
