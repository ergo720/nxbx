// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <chrono>


struct cmos_t {
	uint8_t ram[128]; // byte at index 0x7F is the century register on the xbox
	uint8_t reg_idx;
	uint64_t last_update_time;
	uint64_t lost_us;
	std::time_t sys_time;
};

uint8_t cmos_read_handler(uint32_t port, void *opaque);
void cmos_write_handler(uint32_t port, const uint8_t data, void *opaque);
uint64_t cmos_get_next_update_time(uint64_t now);
void cmos_init();
