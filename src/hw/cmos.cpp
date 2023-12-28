// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "cmos.hpp"
#include "cpu.hpp"
#include "../clock.hpp"
#include "../init.hpp"
#include <limits>
#include <assert.h>


static cmos_t g_cmos;

static uint8_t
to_bcd(uint8_t data) // binary -> bcd
{
	if (!(g_cmos.ram[0x0B] & 4)) {
		// Binary format enabled, convert
		uint8_t tens = data / 10;
		uint8_t units = data % 10;
		return (tens << 4) | units;
	}
	
	return data;
}

static uint8_t
from_bcd(uint8_t data) // bcd -> binary
{
	if (g_cmos.ram[0x0B] & 4) {
		// Binary format enabled, don't convert
		return data;
	}

	uint8_t tens = data >> 4;
	uint8_t units = data & 0x0F;
	return (tens * 10) + units;
}

static void
cmos_update_time(uint64_t elapsed_us)
{
	g_cmos.lost_us += elapsed_us;
	uint64_t actual_elapsed_sec = g_cmos.lost_us / ticks_per_second;
	g_cmos.sys_time += actual_elapsed_sec;
	g_cmos.lost_us -= (actual_elapsed_sec * ticks_per_second);
}

uint8_t
cmos_read_handler(uint32_t port, void *opaque)
{
	if (port == 0x71) {
		if ((g_cmos.reg_idx < 0xA) || (g_cmos.reg_idx == 0x7F)) {
			tm local_time;
#ifdef _WIN32
			if (localtime_s(&local_time, &g_cmos.sys_time)) {
				logger("Failed to read CMOS time");
				cpu_exit(g_cpu);
				return 0xFF;
			}
#else
			if (localtime_s(&g_cmos.sys_time, &local_time) == nullptr) {
				logger("Failed to read CMOS time");
				cpu_exit(g_cpu);
				return 0xFF;
			}
#endif
			switch (g_cmos.reg_idx)
			{
			case 1:
			case 3:
			case 5:
				return g_cmos.ram[g_cmos.reg_idx];

			case 0:
				return to_bcd(local_time.tm_sec);

			case 2:
				return to_bcd(local_time.tm_min);

			case 4:
				if (!(g_cmos.ram[0xB] & 2)) {
					// 12 hour format enabled
					if (local_time.tm_hour == 0) {
						return to_bcd(12);
					}
					else if (local_time.tm_hour > 11) {
						// time is pm
						if (local_time.tm_hour != 12) {
							return to_bcd(local_time.tm_hour - 12) | 0x80;
						}
						return to_bcd(local_time.tm_hour) | 0x80;
					}
				}
				return to_bcd(local_time.tm_hour);

			case 6:
				return to_bcd(local_time.tm_wday + 1);

			case 7:
				return to_bcd(local_time.tm_mday);

			case 8:
				return to_bcd(local_time.tm_mon + 1);

			case 9:
				return to_bcd(local_time.tm_year % 100);

			case 0x7F:
				return to_bcd(g_cmos.ram[0x7F]);
			}

			assert(0);

			return 0xFF;
		}
		else if (g_cmos.reg_idx == 0xC) {
			g_cmos.ram[0x0C] = 0x00; // clears all interrupt flags
		}
		else if (g_cmos.reg_idx == 0xD) {
			g_cmos.ram[0xD] = 0x80; // set VRT
		}

		return g_cmos.ram[g_cmos.reg_idx];
	}

	return 0xFF;
}

void
cmos_write_handler(uint32_t port, const uint8_t data, void *opaque)
{
	uint8_t data1 = data;

	switch (port)
	{
	case 0x70:
		g_cmos.reg_idx = data;
		break;

	case 0x71:
		if ((g_cmos.reg_idx < 0xA) || (g_cmos.reg_idx == 0x7F)) {
			tm local_time;
#ifdef _WIN32
			if (localtime_s(&local_time, &g_cmos.sys_time)) {
				logger("Failed to update CMOS time");
				cpu_exit(g_cpu);
				return;
			}
#else
			if (localtime_s(&g_cmos.sys_time, &local_time) == nullptr) {
				logger("Failed to update CMOS time");
				cpu_exit(g_cpu);
				return;
			}
#endif

			switch (g_cmos.reg_idx)
			{
			case 1:
			case 3:
			case 5:
				g_cmos.ram[g_cmos.reg_idx] = data1;
				break;

			case 0:
				local_time.tm_sec = from_bcd(data1);
				break;

			case 2:
				local_time.tm_min = from_bcd(data1);
				break;

			case 4: {
				uint8_t masked_data = data1 & 0x7F;
				local_time.tm_hour = from_bcd(masked_data);
				if (!(g_cmos.ram[0xB] & 2)) {
					// 12 hour format enabled
					if (data & 0x80) {
						// time is pm
						if (masked_data < 12) {
							local_time.tm_hour += 12;
						}
					}
					else if (masked_data == 12) {
						// it's 12 am
						local_time.tm_hour = 0;
					}
				}
			}
			break;

			case 6:
				local_time.tm_wday = from_bcd(data1) - 1;
				break;

			case 7:
				local_time.tm_mday = from_bcd(data1);
				break;

			case 8:
				local_time.tm_mon = from_bcd(data1) - 1;
				break;

			case 9:
				local_time.tm_year = (g_cmos.ram[0x7F] * 100 - 1900) + from_bcd(data1);
				break;

			case 0x7F:
				g_cmos.ram[0x7F] = from_bcd(data1);
				break;
			}

			g_cmos.sys_time = std::mktime(&local_time);
			if (g_cmos.sys_time == -1) {
				logger("Failed to update CMOS time");
				cpu_exit(g_cpu);
				return;
			}
		}
		else {
			switch (g_cmos.reg_idx)
			{
			case 0xA:
				data1 &= ~0x80; // UIP is read-only
				break;

			case 0xB:
				if (data1 & 0x78) {
					logger(log_lv::error, "CMOS interrupts and square wave outputs are not supported");
					cpu_exit(g_cpu);
				}
				else if (data1 & 0x80) {
					g_cmos.ram[0xA] &= ~0x80; // clears UIP
				}
				break;

			case 0xC:
			case 0xD:
				// Registers C and D are read-only
				return;

			default:
				if (g_cmos.reg_idx >= sizeof(g_cmos.ram)) {
					logger(log_lv::warn, "CMOS write: unknown register %u", g_cmos.reg_idx);
					return;
				}
			}

			g_cmos.ram[g_cmos.reg_idx] = data1;
		}
		break;
	}
}

uint64_t
cmos_get_next_update_time(uint64_t now)
{
	uint64_t next_time;
	if (g_cmos.ram[0xB] & 0x80) { // "SET" bit disables clock updates
		return std::numeric_limits<uint64_t>::max();
	}
	else {
		if (now - g_cmos.last_update_time >= ticks_per_second) {
			g_cmos.last_update_time = now;
			next_time = ticks_per_second;
			cmos_update_time(now - g_cmos.last_update_time);
		}
		else {
			next_time = g_cmos.last_update_time + ticks_per_second - now;
		}

		return next_time;
	}
}

static void
cmos_reset()
{
	g_cmos.ram[0x0B] &= ~0x78; // clears interrupt enable and square wave output flags
	g_cmos.ram[0x0C] = 0x00; // clears all interrupt flags
}

void
cmos_init()
{
	g_cmos.ram[0x0A] = 0x26;
	g_cmos.ram[0x0B] = 0x02;
	g_cmos.ram[0x0C] = 0x00;
	g_cmos.ram[0x0D] = 0x80;
	g_cmos.lost_us = 0;
	g_cmos.last_update_time = get_now();
	g_cmos.sys_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // TODO: this should be read form a ini file instead
	add_reset_func(cmos_reset);
}
