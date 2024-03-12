// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "machine.hpp"
#include "../clock.hpp"
#include <assert.h>


uint8_t
cmos::to_bcd(uint8_t data) // binary -> bcd
{
	if (!(ram[0x0B] & 4)) {
		// Binary format enabled, convert
		uint8_t tens = data / 10;
		uint8_t units = data % 10;
		return (tens << 4) | units;
	}
	
	return data;
}

uint8_t
cmos::from_bcd(uint8_t data) // bcd -> binary
{
	if (ram[0x0B] & 4) {
		// Binary format enabled, don't convert
		return data;
	}

	uint8_t tens = data >> 4;
	uint8_t units = data & 0x0F;
	return (tens * 10) + units;
}

void
cmos::update_time(uint64_t elapsed_us)
{
	lost_us += elapsed_us;
	uint64_t actual_elapsed_sec = lost_us / timer::ticks_per_second;
	sys_time += actual_elapsed_sec;
	lost_us -= (actual_elapsed_sec * timer::ticks_per_second);
}

uint8_t
cmos::read_handler(uint32_t port)
{
	if (port == 0x71) {
		if ((reg_idx < 0xA) || (reg_idx == 0x7F)) {
			tm local_time;
#ifdef _WIN32
			if (localtime_s(&local_time, &sys_time)) {
				nxbx::fatal("Failed to read CMOS time");
				return 0xFF;
			}
#else
			if (localtime_s(&sys_time, &local_time) == nullptr) {
				nxbx::fatal("Failed to read CMOS time");
				return 0xFF;
			}
#endif
			switch (reg_idx)
			{
			case 1:
			case 3:
			case 5:
				return ram[reg_idx];

			case 0:
				return to_bcd(local_time.tm_sec);

			case 2:
				return to_bcd(local_time.tm_min);

			case 4:
				if (!(ram[0xB] & 2)) {
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
				return to_bcd((local_time.tm_year + 1900) / 100);
			}

			assert(0);

			return 0xFF;
		}
		else if (reg_idx == 0xC) {
			ram[0x0C] = 0x00; // clears all interrupt flags
		}
		else if (reg_idx == 0xD) {
			ram[0xD] = 0x80; // set VRT
		}

		return ram[reg_idx];
	}

	return 0xFF;
}

void
cmos::write_handler(uint32_t port, const uint8_t data)
{
	uint8_t data1 = data;

	switch (port)
	{
	case 0x70:
		reg_idx = data;
		break;

	case 0x71:
		if ((reg_idx < 0xA) || (reg_idx == 0x7F)) {
			tm local_time;
#ifdef _WIN32
			if (localtime_s(&local_time, &sys_time)) {
				nxbx::fatal("Failed to update CMOS time");
				return;
			}
#else
			if (localtime_s(&sys_time, &local_time) == nullptr) {
				nxbx::fatal("Failed to update CMOS time");
				return;
			}
#endif

			switch (reg_idx)
			{
			case 1:
			case 3:
			case 5:
				ram[reg_idx] = data1;
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
				if (!(ram[0xB] & 2)) {
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
				local_time.tm_year = (ram[0x7F] * 100 - 1900) + from_bcd(data1);
				break;

			case 0x7F:
				ram[0x7F] = from_bcd(data1);
				break;
			}

			if (time_t time = std::mktime(&local_time); time == -1) {
				nxbx::fatal("Failed to update CMOS time");
				return;
			}
			else {
				sys_time = time;
				sys_time_bias = time - std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			}
		}
		else {
			switch (reg_idx)
			{
			case 0xA:
				data1 &= ~0x80; // UIP is read-only
				break;

			case 0xB:
				if (data1 & 0x78) {
					nxbx::fatal("CMOS interrupts and square wave outputs are not supported");
				}
				else if (data1 & 0x80) {
					ram[0xA] &= ~0x80; // clears UIP
				}
				break;

			case 0xC:
			case 0xD:
				// Registers C and D are read-only
				return;

			default:
				if (reg_idx >= sizeof(ram)) {
					logger(log_lv::warn, "CMOS write: unknown register %u", reg_idx);
					return;
				}
			}

			ram[reg_idx] = data1;
		}
		break;
	}
}

uint64_t
cmos::get_next_update_time(uint64_t now)
{
	uint64_t next_time;
	if (ram[0xB] & 0x80) { // "SET" bit disables clock updates
		return std::numeric_limits<uint64_t>::max();
	}
	else {
		if (now - last_update_time >= timer::ticks_per_second) {
			last_update_time = now;
			next_time = timer::ticks_per_second;
			update_time(now - last_update_time);
		}
		else {
			next_time = last_update_time + timer::ticks_per_second - now;
		}

		return next_time;
	}
}

void
cmos::reset()
{
	ram[0x0B] &= ~0x78; // clears interrupt enable and square wave output flags
	ram[0x0C] = 0x00; // clears all interrupt flags
}

bool
cmos::init()
{
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x70, 2, true,
		{
			.fnr8 = cpu_read<cmos, uint8_t, &cmos::read_handler>,
			.fnw8 = cpu_write<cmos, uint8_t, &cmos::write_handler>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s io ports", get_name());
		return false;
	}

	ram[0x0A] = 0x26;
	ram[0x0B] = 0x02;
	ram[0x0C] = 0x00;
	ram[0x0D] = 0x80;
	lost_us = 0;
	last_update_time = timer::get_now();
	sys_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + nxbx::get_settings<core_s>().sys_time_bias;
	return true;
}

void
cmos::deinit()
{
	nxbx::get_settings<core_s>().sys_time_bias = sys_time_bias;
}
