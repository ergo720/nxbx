// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../../util.hpp"
#include "../../../clock.hpp"
#include "machine.hpp"

#define COUNTER_ON 2 // use 2 instead of 1 so that the value can be ORed with the int_enabled register
#define COUNTER_OFF 0

#define NV_PTIMER 0x00009000
#define NV_PTIMER_BASE (NV2A_REGISTER_BASE + NV_PTIMER)
#define NV_PTIMER_SIZE 0x1000

#define NV_PTIMER_INTR_0 (NV2A_REGISTER_BASE + 0x00009100)
#define NV_PTIMER_INTR_0_ALARM_NOT_PENDING 0x00000000
#define NV_PTIMER_INTR_0_ALARM_PENDING 0x00000001
#define NV_PTIMER_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00009140)
#define NV_PTIMER_INTR_EN_0_ALARM_DISABLED 0x00000000
#define NV_PTIMER_INTR_EN_0_ALARM_ENABLED 0x00000001
#define NV_PTIMER_NUMERATOR (NV2A_REGISTER_BASE + 0x00009200)
#define NV_PTIMER_NUMERATOR_MASK 0xFFFF
#define NV_PTIMER_DENOMINATOR (NV2A_REGISTER_BASE + 0x00009210)
#define NV_PTIMER_DENOMINATOR_MASK 0xFFFF
#define NV_PTIMER_TIME_0 (NV2A_REGISTER_BASE + 0x00009400)
#define NV_PTIMER_TIME_1 (NV2A_REGISTER_BASE + 0x00009410)
#define NV_PTIMER_ALARM_0 (NV2A_REGISTER_BASE + 0x00009420)


uint64_t
ptimer::counter_to_us()
{
	// Tested on a Retail 1.0 xbox: the ratio is calculated with denominator / numerator, and not the other way around like it might seem at first. The gpu documentation
	// from envytools also indicates this. Also, the alarm value has no effect on the counter period, which is only affected by the ratio instead
	constexpr uint64_t max_alarm = 0xFFFFFFE0 >> 5;
	return util::muldiv128(util::muldiv128(max_alarm, timer::ticks_per_second, m_machine->get<pramdac>().core_freq), divider, multiplier);
}

uint64_t
ptimer::get_next_alarm_time(uint64_t now)
{
	if (((int_enabled & NV_PTIMER_INTR_EN_0_ALARM_ENABLED) | counter_active) == (NV_PTIMER_INTR_EN_0_ALARM_ENABLED | COUNTER_ON)) {
		uint64_t next_time, ptimer_period = counter_period;
		if (int64_t(now - last_alarm_time) >= ((int64_t)ptimer_period + counter_bias)) {
			counter_bias = 0;
			last_alarm_time = now;
			next_time = ptimer_period;

			int_status |= NV_PTIMER_INTR_0_ALARM_PENDING;
			m_machine->get<pmc>().update_irq();
		}
		else {
			next_time = last_alarm_time + ptimer_period - now;
		}

		return next_time;
	}

	return std::numeric_limits<uint64_t>::max();
}

void
ptimer::write(uint32_t addr, const uint32_t data)
{
	switch (addr)
	{
	case NV_PTIMER_INTR_0:
		int_status &= ~data;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PTIMER_INTR_EN_0:
		int_enabled = data;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PTIMER_NUMERATOR:
		divider = data & NV_PTIMER_NUMERATOR_MASK;
		if (counter_active) {
			counter_period = counter_to_us();
			cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(timer::get_now()));
		}
		break;

	case NV_PTIMER_DENOMINATOR: {
		multiplier = data & NV_PTIMER_DENOMINATOR_MASK;
		if (multiplier > divider) [[unlikely]] {
			// Testing on a Retail 1.0 xbox shows that, when this condition is hit, the console hangs. We don't actually want to freeze the emulator, so
			// we will just terminate the emulation instead
			nxbx::fatal("Invalid ratio multiplier -> multiplier > divider (the real hardware would hang here)");
		}
		counter_active = multiplier ? COUNTER_ON : COUNTER_OFF; // A multiplier of zero stops the 56 bit counter
		uint64_t now = timer::get_now();
		if (counter_active) {
			counter_period = counter_to_us();
			last_alarm_time = now;
		}
		else {
			counter_when_stopped = timer::get_dev_now(m_machine->get<pramdac>().core_freq) & 0xFFFFFFFFFFFFFF;
		}
		cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(now));
	}
	break;

		// Tested on a Retail 1.0 xbox: writing to the NV_PTIMER_TIME_0/1 registers causes the timer to start counting from the written value
	case NV_PTIMER_TIME_0:
		counter_offset = (counter_offset & (0xFFFFFFFFULL << 32)) | data;
		break;

	case NV_PTIMER_TIME_1:
		counter_offset = (counter_offset & 0xFFFFFFFFULL) | ((uint64_t)data << 32);
		break;

	case NV_PTIMER_ALARM_0: {
		// Tested on a Retail 1.0 xbox: changing the alarm time doesn't change the frequency at which the alarm triggers, only changing the numerator and denominator can do that.
		// This is because the counter merely counts from 0 up to 2^32 - 1, in increments of 32, and triggers once per cycle when alarm == counter. Changing the
		// alarm time has the side effect that it might trigger sooner or later for the next cycle, but after that only once per cycle again
		/*
		* n=now, a=old alarm, a1=new alarm
		------------------------ ------------------------ period
		          0                          0
		a                        a                      n
		   a1                       a1                  n bias- (period smaller for one cycle)
		                    a1                          n bias+ (period larger for one cycle)
		*/
		uint32_t old_alarm = alarm >> 5;
		alarm = data & ~0x1F; // tested on hw: writes of 1s to the first five bits have no impact
		uint32_t new_alarm = alarm >> 5;
		counter_bias = new_alarm - old_alarm;
		if (counter_active) {
			cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(timer::get_now()));
		}
	}
	break;

	default:
		nxbx::fatal("Unhandled %s write at address 0x%" PRIX32 " with value 0x%" PRIX32, get_name(), addr, data);
	}
}

uint32_t
ptimer::read(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PTIMER_INTR_0:
		value = int_status;
		break;

	case NV_PTIMER_INTR_EN_0:
		value = int_enabled;
		break;

	case NV_PTIMER_NUMERATOR:
		value = divider;
		break;

	case NV_PTIMER_DENOMINATOR:
		value = multiplier;
		break;

	case NV_PTIMER_TIME_0: {
		// Returns the low 27 bits of the 56 bit counter
		uint64_t counter_base = counter_active ? timer::get_dev_now(m_machine->get<pramdac>().core_freq) : counter_when_stopped;
		value = uint32_t(((counter_offset + counter_base) & 0x7FFFFFF) << 5);
	}
	break;

	case NV_PTIMER_TIME_1: {
		// Returns the high 29 bits of the 56 bit counter
		uint64_t counter_base = counter_active ? timer::get_dev_now(m_machine->get<pramdac>().core_freq) : counter_when_stopped;
		value = uint32_t(((counter_offset + counter_base) >> 27) & 0x1FFFFFFF);
	}
	break;

	case NV_PTIMER_ALARM_0:
		value = alarm;
		break;

	default:
		nxbx::fatal("Unhandled %s read at address 0x%" PRIX32, get_name(), addr);
	}

	return value;
}

void
ptimer::reset()
{
	// Values dumped from a Retail 1.0 xbox
	int_status = NV_PTIMER_INTR_0_ALARM_NOT_PENDING;
	int_enabled = NV_PTIMER_INTR_EN_0_ALARM_DISABLED;
	multiplier = 0x00001DCD;
	divider = 0x0000DE86;
	alarm = 0xFFFFFFE0;
	counter_period = 0;
	counter_active = COUNTER_ON;
	counter_offset = 0;
	counter_bias = 0;
}

bool
ptimer::init()
{
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PTIMER_BASE, NV_PTIMER_SIZE, false,
		{
			.fnr32 = cpu_read<ptimer, uint32_t, &ptimer::read>,
			.fnw32 = cpu_write<ptimer, uint32_t, &ptimer::write>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio region", get_name());
		return false;
	}

	reset();
	return true;
}
