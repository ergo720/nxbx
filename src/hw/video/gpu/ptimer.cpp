// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../../util.hpp"
#include "../../../clock.hpp"
#include "machine.hpp"

#define NV_PTIMER 0x00009000
#define NV_PTIMER_BASE (NV2A_REGISTER_BASE + NV_PTIMER)
#define NV_PTIMER_SIZE 0x1000

#define NV_PTIMER_INTR_0 (NV2A_REGISTER_BASE + 0x00009100)
#define NV_PTIMER_INTR_0_ALARM_NOT_PENDING 0x00000000
#define NV_PTIMER_INTR_0_ALARM_PENDING 0x00000001
#define NV_PTIMER_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00009140)
#define NV_PTIMER_INTR_EN_0_ALARM_DISABLED 0x00000000
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
	return util::muldiv128(util::muldiv128(alarm >> 5, timer::ticks_per_second, m_machine->get<pramdac>().core_freq),
		divider & NV_PTIMER_DENOMINATOR_MASK, multiplier & NV_PTIMER_NUMERATOR_MASK);
}

uint64_t
ptimer::get_next_alarm_time(uint64_t now)
{
	if (counter_active) {
		uint64_t next_time, ptimer_period = counter_period;
		if (now - last_alarm_time >= ptimer_period) {
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

	case NV_PTIMER_NUMERATOR: {
		// A multiplier of zero stops the 56 bit counter
		multiplier = data;
		counter_active = multiplier & NV_PTIMER_NUMERATOR_MASK ? 1 : 0;
		uint64_t now = timer::get_now();
		if (counter_active) {
			counter_period = counter_to_us();
			last_alarm_time = now;
		}
		cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(timer::get_now()));
	}
	break;

	case NV_PTIMER_DENOMINATOR:
		divider = data;
		if (counter_active) {
			counter_period = counter_to_us();
			cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(timer::get_now()));
		}
		break;

		// Tested on a Retail 1.0 xbox: writing to the NV_PTIMER_TIME_0/1 registers causes the timer to start counting from the written value
	case NV_PTIMER_TIME_0:
		counter_offset = (counter_offset & (0xFFFFFFFFULL << 32)) | data;
		break;

	case NV_PTIMER_TIME_1:
		counter_offset = (counter_offset & 0xFFFFFFFFULL) | ((uint64_t)data << 32);
		break;

	case NV_PTIMER_ALARM_0:
		alarm = data;
		if (counter_active) {
			counter_period = counter_to_us();
			cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(timer::get_now()));
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
		value = multiplier;
		break;

	case NV_PTIMER_DENOMINATOR:
		value = divider;
		break;

	case NV_PTIMER_TIME_0:
		// Returns the low 27 bits of the 56 bit counter
		value = uint32_t(counter_offset + ((timer::get_dev_now(m_machine->get<pramdac>().core_freq) & 0x7FFFFFF) << 5));
		break;

	case NV_PTIMER_TIME_1:
		// Returns the high 29 bits of the 56 bit counter
		value = uint32_t(counter_offset + ((timer::get_dev_now(m_machine->get<pramdac>().core_freq) >> 27) & 0x1FFFFFFF));
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
	int_status = NV_PTIMER_INTR_0_ALARM_NOT_PENDING;
	int_enabled = NV_PTIMER_INTR_EN_0_ALARM_DISABLED;
	multiplier = 0;
	divider = 1;
	alarm = 0;
	counter_period = 0;
	counter_active = 0;
	counter_offset = 0;
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
		logger(log_lv::error, "Failed to initialize %s mmio ports", get_name());
		return false;
	}

	reset();
	return true;
}
