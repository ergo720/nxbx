// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "machine.hpp"
#include "../clock.hpp"

#define MODULE_NAME cmos

#define CMOS_FREQ 32768 // in Hz
#define UIP_PERIOD 244 // in us
#define CMOS_IRQ_NUM 8

#define B_SET 0x80 // update cycle enable
#define B_PIE 0x40 // periodic interrupt enable
#define B_AIE 0x20 // alarm interrupt enable
#define B_UIE 0x10 // update-ended interrupt enable
#define C_IRQF 0x80 // interrupt request flag
#define C_PF 0x40 // periodic interrupt flag
#define C_AF 0x20 // alarm interrupt flag
#define C_UF 0x10 // update-ended interrupt flag


uint8_t
cmos::to_bcd(uint8_t value) // binary -> bcd
{
	if (!(m_ram[0x0B] & 4)) {
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
	if (m_ram[0x0B] & 4) {
		// Binary format enabled, don't convert
		return value;
	}

	uint8_t tens = value >> 4;
	uint8_t units = value & 0x0F;
	return (tens * 10) + units;
}

uint8_t cmos::read(uint8_t idx)
{
	uint8_t value = 0;

	if ((idx < 0xA) || (idx == 0x7F)) {
		tm *local_time;
		if (local_time = std::localtime(&m_sys_time); local_time == nullptr) {
			nxbx_fatal("Failed to read CMOS time");
			return 0;
		}

		switch (idx)
		{
		case 1:
		case 3:
		case 5:
			value = m_ram[idx];
			break;

		case 0:
			value = to_bcd(local_time->tm_sec);
			break;

		case 2:
			value = to_bcd(local_time->tm_min);
			break;

		case 4:
			if (!(m_ram[0xB] & 2)) {
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
	else if (idx == 0xA) {
		// Special case for UIP bit
		//                                    A                           C     B
		//                                    v                           v     v
		//  |---------------------------======|---------------------------======|
		//  ^                           ^     ^                           ^     ^
		//  0                          UIP    1                          UIP    2
		//
		// A: cmos.last_second_update
		// B: cmos.last_second_update + ticks_per_second
		// B <==> C: cmos.uip_period
		// C: cmos.last_second_update + ticks_per_second - cmos.uip_period
		// UIP will be set if it's within regions B and C.

		value = m_ram[0xA];
		uint64_t now = timer::get_now();
		uint64_t next_second = m_last_clock + timer::ticks_per_second;
		if ((now >= (next_second - UIP_PERIOD)) && (now < next_second)) {
			value |= 0x80; // UIP bit is still set.
		}
	}
	else if (idx == 0xB) {
		value = m_ram[0xB];
	}
	else if (idx == 0xC) {
		m_machine->lower_irq(CMOS_IRQ_NUM);
		value = m_ram[0xC];
		m_ram[0x0C] = 0x00; // clears all interrupt flags
	}
	else if (idx == 0xD) {
		value = m_ram[0xD]; // always reads as 0x80
	}
	else {
		value = m_ram[idx];
	}

	return value;
}

template<bool log>
uint8_t cmos::read8(uint32_t addr)
{
	uint8_t value = 0;

	if (addr == CMOS_PORT_DATA) {
		value = read(m_reg_idx);
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
		m_reg_idx = value;
		break;

	case CMOS_PORT_DATA:
		if ((m_reg_idx < 0xA) || (m_reg_idx == 0x7F)) {
			tm *local_time;
			if (local_time = std::localtime(&m_sys_time); local_time == nullptr) {
				nxbx_fatal("Failed to read CMOS time");
				return;
			}

			switch (m_reg_idx)
			{
			case 1:
			case 3:
			case 5:
				m_ram[m_reg_idx] = value1;
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
				if (!(m_ram[0xB] & 2)) {
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
				local_time->tm_year = (m_ram[0x7F] * 100 - 1900) + from_bcd(value1);
				break;

			case 0x7F:
				m_ram[0x7F] = from_bcd(value1);
				break;
			}

			if (time_t time = std::mktime(local_time); time == -1) {
				nxbx_fatal("Failed to update CMOS time");
				return;
			}
			else {
				m_sys_time = time;
				m_sys_time_bias = time - std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			}
		}
		else {
			switch (m_reg_idx)
			{
			case 0xA:
				m_ram[m_reg_idx] = value1 & ~0x80; // UIP is read-only
				update_timer();
				break;

			case 0xB:
				m_ram[m_reg_idx] = value1;
				update_timer();
				break;

			case 0xC:
			case 0xD:
				// Registers C and D are read-only
				break;

			default:
				m_ram[m_reg_idx] = value1;
			}
		}
		break;
	}
}

void
cmos::raise_irq(uint8_t why)
{
	m_ram[0xC] = C_IRQF | why;
	m_machine->raise_irq(CMOS_IRQ_NUM);
}

void
cmos::update_timer()
{
	uint64_t now = timer::get_now();
	uint8_t old_int_state = m_int_running;
	uint8_t old_clock_state = m_clock_running;

	uint64_t period = m_ram[0xA] & 0xF; // RS[0-3] bits (establish final freq to use)
	if (!period) {
		if (m_int_running) {
			m_int_running = 0;
		}

		if ((m_clock_running == 0) && (m_ram[0xB] & B_SET) == 0) {
			m_clock_running = 1;
			m_last_clock = now;
		}
		else if ((m_clock_running == 1) && (m_ram[0xB] & B_SET)) { // "SET" bit disables clock updates
			m_clock_running = 0;
		}
	}
	else {
		if ((m_int_running == 0) && (m_ram[0xB] & B_PIE)) {
			if ((((m_ram[0xA] >> 4) & 7) == 2) && (period < 3)) {
				period += 7; // for 32768 Hz base freq only, changes 256/128 Hz freq of RS[0-3] with bits = 1,2 to their equivalents at bits = 8,9
			}

			uint64_t freq = CMOS_FREQ >> (period - 1); // actual interrupt frequency in Hz

			m_int_running = 1;
			m_period_int = timer::ticks_per_second / freq; // period of the currently selected periodic interrupt rate, in us
			m_last_int = now;
			m_periodic_ticks = 0;
			m_periodic_ticks_max = freq; // number of periodic interrupts in 1 sec
		}
		else if ((m_int_running == 1) && ((m_ram[0xB] & B_PIE) == 0)) {
			m_int_running = 0;
		}

		if ((m_clock_running == 0) && (m_ram[0xB] & B_SET) == 0) {
			m_clock_running = 1;
			m_last_clock = now;
		}
		else if ((m_clock_running == 1) && (m_ram[0xB] & B_SET)) { // "SET" bit disables clock updates
			m_clock_running = 0;
		}
	}

	if ((old_int_state ^ m_int_running) | (old_clock_state ^ m_clock_running)) {
		cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(now));
	}
}

void
cmos::update_clock(uint64_t elapsed_us)
{
	m_lost_us += elapsed_us;
	uint64_t actual_elapsed_sec = m_lost_us / timer::ticks_per_second;
	m_sys_time += actual_elapsed_sec;
	m_lost_us -= (actual_elapsed_sec * timer::ticks_per_second);
}

uint64_t
cmos::update_periodic_ticks(uint64_t elapsed_us)
{
	m_lost_ticks += elapsed_us;
	uint64_t actual_elapsed_ticks = m_lost_ticks / m_period_int;
	m_lost_ticks -= (actual_elapsed_ticks * m_period_int);
	return actual_elapsed_ticks;
}

uint64_t
cmos::get_next_update_time(uint64_t now)
{
	// Some things to deal with:
	//  - Periodic interrupt
	//  - Updating seconds
	//  - Alarm Interrupt
	//  - UIP Interrupt (basically every second)
	// Note that one or more of these can happen per cmos_clock (required by OS/2 Warp 4.5)
	// Also sets UIP timer (needed for Windows XP timing calibration loop)

	// We have two options when it comes to CMOS timing: we can update registers per second or per interrupt.
	// If the periodic interrupt is not enabled, then we only have to update the clock every second.
	// If the periodic interrupt is enabled, then there's no reason to update the clock every second AND check
	// for the periodic interrupt -- every Nth periodic interrupt, there will be a clock update.

	if (m_int_running | m_clock_running) {
		if (m_int_running) {
			uint64_t next_int = m_last_int + m_period_int;
			if (now >= next_int) {
				uint8_t why = 0;

				if (m_ram[0x0B] & B_PIE) {
					// Periodic interrupt is enabled.
					why |= C_PF;

					// Every Nth periodic interrupt, we will cause an alarm/UIP interrupt.
					m_periodic_ticks = update_periodic_ticks(now - m_last_int);
					if (m_periodic_ticks != m_periodic_ticks_max) {
						m_last_int = now; // No, we haven't reached the Nth tick yet
						raise_irq(why);
						return m_period_int;
					}

					m_periodic_ticks = 0; // Reset it back to zero since m_periodic_ticks == m_periodic_ticks_max
				}

				// Otherwise, we're here to update seconds.
				if (m_ram[0x0B] & B_AIE) {
					uint8_t alarm = 0;
					uint8_t time_now[3];
					uint8_t dont_care[3];
					for (uint8_t i = 0; i < 6; i += 2) {
						time_now[i >> 1] = read(i);
						uint8_t mask = dont_care[i >> 1] = (m_ram[i + 1] & 0xC0) == 0xC0 ? 0x3F : 0xFF;
						alarm |= ((m_ram[i + 1] ^ time_now[i >> 1]) & mask); // compares alarm against now
					}
					if (alarm == 0) {
						why |= C_AF;
					}
				}

				if (m_ram[0x0B] & B_UIE) {
					// Clock has completed an update cycle
					why |= C_UF;
				}

				update_clock(now - m_last_clock);
				m_last_clock = now; // we just updated the seconds

				if (why) {
					raise_irq(why);
				}

				return m_int_running ? m_period_int : timer::ticks_per_second;
			}

			return next_int - now;
		}
		else {
			uint64_t next_clock = m_last_clock + timer::ticks_per_second;
			if (now >= next_clock) {
				update_clock(now - m_last_clock);
				m_last_clock = now; // we just updated the seconds

				return timer::ticks_per_second;
			}

			return next_clock - now;
		}
	}

	return std::numeric_limits<uint64_t>::max();
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
	m_ram[0x0B] &= ~0x78; // clears interrupt enable and square wave output flags
	m_ram[0x0C] = 0x00; // clears all interrupt flags
}

bool
cmos::init()
{
	if (!update_io(false)) {
		return false;
	}

	m_ram[0x0A] = 0x26;
	m_ram[0x0B] = 0x02;
	m_ram[0x0C] = 0x00;
	m_ram[0x0D] = 0x80;
	m_lost_us = m_lost_ticks = 0;
	m_periodic_ticks = m_periodic_ticks_max = 0;
	m_int_running = 0;
	m_clock_running = 1;
	m_last_int = m_last_clock = timer::get_now();
	m_sys_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + nxbx::get_settings<core_s>().sys_time_bias;
	return true;
}

void
cmos::deinit()
{
	nxbx::get_settings<core_s>().sys_time_bias = m_sys_time_bias;
}
