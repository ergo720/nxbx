// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../../util.hpp"
#include "../../../clock.hpp"
#include "../../cpu.hpp"
#include "nv2a.hpp"

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


static inline uint64_t
ptimer_counter_to_us()
{
	// FIXME: instead of hardcoding the gpu frequency, it should be read from PRAMDAC, since it can be changed from the NV_PRAMDAC_NVPLL_COEFF register
	return muldiv128_(muldiv128_(g_nv2a.ptimer.alarm >> 5, ticks_per_second, NV2A_CLOCK_FREQ),
		g_nv2a.ptimer.divider & NV_PTIMER_DENOMINATOR_MASK, g_nv2a.ptimer.multiplier & NV_PTIMER_NUMERATOR_MASK);
}

uint64_t
ptimer_get_next_alarm_time(uint64_t now)
{
	if (g_nv2a.ptimer.counter_active) {
		uint64_t next_time, ptimer_period = g_nv2a.ptimer.counter_period;
		if (now - g_nv2a.ptimer.last_alarm_time >= ptimer_period) {
			g_nv2a.ptimer.last_alarm_time = now;
			next_time = ptimer_period;

			g_nv2a.ptimer.int_status |= NV_PTIMER_INTR_0_ALARM_PENDING;
			pmc_update_irq();
		}
		else {
			next_time = g_nv2a.ptimer.last_alarm_time + ptimer_period - now;
		}

		return next_time;
	}

	return std::numeric_limits<uint64_t>::max();
}

static void
ptimer_write(uint32_t addr, const uint32_t data, void *opaque)
{
	switch (addr)
	{
	case NV_PTIMER_INTR_0:
		g_nv2a.ptimer.int_status &= ~data;
		pmc_update_irq();
		break;

	case NV_PTIMER_INTR_EN_0:
		g_nv2a.ptimer.int_enabled = data;
		pmc_update_irq();
		break;

	case NV_PTIMER_NUMERATOR: {
		// A multiplier of zero stops the 56 bit counter
		g_nv2a.ptimer.multiplier = data;
		g_nv2a.ptimer.counter_active = g_nv2a.ptimer.multiplier & NV_PTIMER_NUMERATOR_MASK ? 1 : 0;
		uint64_t now = get_now();
		if (g_nv2a.ptimer.counter_active) {
			g_nv2a.ptimer.counter_period = ptimer_counter_to_us();
			g_nv2a.ptimer.last_alarm_time = now;
		}
		cpu_set_timeout(g_cpu, cpu_check_periodic_events(now));
	}
	break;

	case NV_PTIMER_DENOMINATOR:
		g_nv2a.ptimer.divider = data;
		if (g_nv2a.ptimer.counter_active) {
			g_nv2a.ptimer.counter_period = ptimer_counter_to_us();
			cpu_set_timeout(g_cpu, cpu_check_periodic_events(get_now()));
		}
		break;

	case NV_PTIMER_TIME_0:
		// Ignored
		logger(log_lv::info, "%s: ignoring write to NV_PTIMER_TIME_0 (value was 0x%X)", __func__, data);
		break;

	case NV_PTIMER_TIME_1:
		// Ignored
		logger(log_lv::info, "%s: ignoring write to NV_PTIMER_TIME_1 (value was 0x%X)", __func__, data);
		break;

	case NV_PTIMER_ALARM_0:
		g_nv2a.ptimer.alarm = data;
		if (g_nv2a.ptimer.counter_active) {
			g_nv2a.ptimer.counter_period = ptimer_counter_to_us();
			cpu_set_timeout(g_cpu, cpu_check_periodic_events(get_now()));
		}
		break;

	default:
		nxbx_fatal("Unhandled PTIMER write at address 0x%X with value 0x%X", addr, data);
	}
}

static uint32_t
ptimer_read(uint32_t addr, void *opaque)
{
	uint32_t value = std::numeric_limits<uint32_t>::max();

	switch (addr)
	{
	case NV_PTIMER_INTR_0:
		value = g_nv2a.ptimer.int_status;
		break;

	case NV_PTIMER_INTR_EN_0:
		value = g_nv2a.ptimer.int_enabled;
		break;

	case NV_PTIMER_NUMERATOR:
		value = g_nv2a.ptimer.multiplier;
		break;

	case NV_PTIMER_DENOMINATOR:
		value = g_nv2a.ptimer.divider;
		break;

	case NV_PTIMER_TIME_0:
		// Returns the low 27 bits of the 56 bit counter
		// FIXME: instead of hardcoding the gpu frequency, it should be read from PRAMDAC, since it can be changed from the NV_PRAMDAC_NVPLL_COEFF register
		value = (get_dev_now(NV2A_CLOCK_FREQ) & 0x7FFFFFF) << 5;
		break;

	case NV_PTIMER_TIME_1:
		// Returns the high 29 bits of the 56 bit counter
		// FIXME: instead of hardcoding the gpu frequency, it should be read from PRAMDAC, since it can be changed from the NV_PRAMDAC_NVPLL_COEFF register
		value = (get_dev_now(NV2A_CLOCK_FREQ) >> 27) & 0x1FFFFFFF;
		break;

	case NV_PTIMER_ALARM_0:
		value = g_nv2a.ptimer.alarm;
		break;

	default:
		nxbx_fatal("Unhandled PTIMER read at address 0x%X", addr);
	}

	return value;
}

static void
ptimer_reset()
{
	g_nv2a.ptimer.int_status = NV_PTIMER_INTR_0_ALARM_NOT_PENDING;
	g_nv2a.ptimer.int_enabled = NV_PTIMER_INTR_EN_0_ALARM_DISABLED;
	g_nv2a.ptimer.multiplier = 0;
	g_nv2a.ptimer.divider = 1;
	g_nv2a.ptimer.alarm = 0;
	g_nv2a.ptimer.counter_period = 0;
	g_nv2a.ptimer.counter_active = 0;
}

void
ptimer_init()
{
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, NV_PTIMER_BASE, NV_PTIMER_SIZE, false, { .fnr32 = ptimer_read, .fnw32 = ptimer_write }, nullptr))) {
		throw nxbx_exp_abort("Failed to initialize ptimer MMIO range");
	}

	ptimer_reset();
}
