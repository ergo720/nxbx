// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "machine.hpp"
#include "../clock.hpp"

#define MODULE_NAME cmos


uint8_t
cmos::to_bcd(uint8_t value) // binary -> bcd
{
	if (!(ram[0x0B] & 4)) {
		// Binary format enabled, convert
		uint8_t tens = value / 10;
		uint8_t units = value % 10;
		return (tens << 4) | units;
	}
	
	return value;
}

uint8_t
cmos::from_bcd(uint8_t value) // bcd -> binary
{
	if (ram[0x0B] & 4) {
		// Binary format enabled, don't convert
		return value;
	}

	uint8_t tens = value >> 4;
	uint8_t units = value & 0x0F;
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

template<bool log>
uint8_t cmos::read8(uint32_t addr)
{
	uint8_t value = 0;

	if (addr == CMOS_PORT_DATA) {
		if ((reg_idx < 0xA) || (reg_idx == 0x7F)) {
			tm *local_time;
			if (local_time = std::localtime(&sys_time); local_time == nullptr) {
				nxbx_fatal("Failed to read CMOS time");
				return 0;
			}

			switch (reg_idx)
			{
			case 1:
			case 3:
			case 5:
				value = ram[reg_idx];
				break;

			case 0:
				value = to_bcd(local_time->tm_sec);
				break;

			case 2:
				value = to_bcd(local_time->tm_min);
				break;

			case 4:
				if (!(ram[0xB] & 2)) {
					// 12 hour format enabled
					if (local_time->tm_hour == 0) {
						value = to_bcd(12);
					}
					else if (local_time->tm_hour > 11) {
						// time is pm
						if (local_time->tm_hour != 12) {
							value = to_bcd(local_time->tm_hour - 12) | 0x80;
						}
						else {
							value = to_bcd(local_time->tm_hour) | 0x80;
						}
					}
				}
				else {
					value = to_bcd(local_time->tm_hour);
				}
				break;

			case 6:
				value = to_bcd(local_time->tm_wday + 1);
				break;

			case 7:
				value = to_bcd(local_time->tm_mday);
				break;

			case 8:
				value = to_bcd(local_time->tm_mon + 1);
				break;

			case 9:
				value = to_bcd(local_time->tm_year % 100);
				break;

			case 0x7F:
				value = to_bcd((local_time->tm_year + 1900) / 100);
				break;
			}
		}
		else if (reg_idx == 0xC) {
			ram[0x0C] = 0x00; // clears all interrupt flags
		}
		else if (reg_idx == 0xD) {
			ram[0xD] = 0x80; // set VRT
		}

		value = ram[reg_idx];
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void cmos::write8(uint32_t addr, const uint8_t value)
{
	if constexpr (log) {
		log_io_write();
	}

	uint8_t value1 = value;

	switch (addr)
	{
	case CMOS_PORT_CMD:
		reg_idx = value;
		break;

	case CMOS_PORT_DATA:
		if ((reg_idx < 0xA) || (reg_idx == 0x7F)) {
			tm *local_time;
			if (local_time = std::localtime(&sys_time); local_time == nullptr) {
				nxbx_fatal("Failed to read CMOS time");
				return;
			}

			switch (reg_idx)
			{
			case 1:
			case 3:
			case 5:
				ram[reg_idx] = value1;
				break;

			case 0:
				local_time->tm_sec = from_bcd(value1);
				break;

			case 2:
				local_time->tm_min = from_bcd(value1);
				break;

			case 4: {
				uint8_t masked_data = value1 & 0x7F;
				local_time->tm_hour = from_bcd(masked_data);
				if (!(ram[0xB] & 2)) {
					// 12 hour format enabled
					if (value & 0x80) {
						// time is pm
						if (masked_data < 12) {
							local_time->tm_hour += 12;
						}
					}
					else if (masked_data == 12) {
						// it's 12 am
						local_time->tm_hour = 0;
					}
				}
			}
			break;

			case 6:
				local_time->tm_wday = from_bcd(value1) - 1;
				break;

			case 7:
				local_time->tm_mday = from_bcd(value1);
				break;

			case 8:
				local_time->tm_mon = from_bcd(value1) - 1;
				break;

			case 9:
				local_time->tm_year = (ram[0x7F] * 100 - 1900) + from_bcd(value1);
				break;

			case 0x7F:
				ram[0x7F] = from_bcd(value1);
				break;
			}

			if (time_t time = std::mktime(local_time); time == -1) {
				nxbx_fatal("Failed to update CMOS time");
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
				value1 &= ~0x80; // UIP is read-only
				break;

			case 0xB:
				if (value1 & 0x78) {
					nxbx_fatal("CMOS interrupts and square wave outputs are not supported");
				}
				else if (value1 & 0x80) {
					ram[0xA] &= ~0x80; // clears UIP
				}
				break;

			case 0xC:
			case 0xD:
				// Registers C and D are read-only
				return;

			default:
				if (reg_idx >= sizeof(ram)) {
					logger_en(warn, "CMOS write: unknown register %u", reg_idx);
					return;
				}
			}

			ram[reg_idx] = value1;
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
			update_time(now - last_update_time);
			last_update_time = now;
			next_time = timer::ticks_per_second;
		}
		else {
			next_time = last_update_time + timer::ticks_per_second - now;
		}

		return next_time;
	}
}

bool
cmos::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x70, 2, true,
		{
			.fnr8 = log ? cpu_read<cmos, uint8_t, &cmos::read8<true>> : cpu_read<cmos, uint8_t, &cmos::read8<false>>,
			.fnw8 = log ? cpu_write<cmos, uint8_t, &cmos::write8<true>> : cpu_write<cmos, uint8_t, &cmos::write8<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update io ports");
		return false;
	}

	return true;
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
	if (!update_io(false)) {
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
