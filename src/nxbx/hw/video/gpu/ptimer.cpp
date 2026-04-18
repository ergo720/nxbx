// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "clock.hpp"
#include "pramdac.hpp"
#include "ptimer.hpp"
#include "pmc.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include "util.hpp"

#define MODULE_NAME ptimer

#define COUNTER_ON 2 // use 2 instead of 1 so that the value can be ORed with the int_enabled register
#define COUNTER_OFF 0


/** Private device implementation **/
class ptimer::Impl
{
public:
	void init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo() { updateIo(true); }
	uint64_t getNextAlarmTime(uint64_t now);
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);
	uint8_t isCounterOn() { return counter_active; }
	void setCounterPeriod(uint64_t new_period) { counter_period = new_period; }
	uint64_t counterToUs();

private:
	void updateIo(bool is_update);
	template<bool is_write>
	auto getIoFunc(bool log, bool enabled, bool is_be);

	// connected devices
	pmc *m_pmc;
	pramdac *m_pramdac;
	cpu *m_cpu;
	cpu_t *m_lc86cpu;
	// Host time when the last alarm interrupt was triggered
	uint64_t last_alarm_time;
	// Time in us before the alarm triggers
	uint64_t counter_period;
	// Bias added/subtracted to counter before an alarm is due
	int64_t counter_bias;
	// Counter is running if not zero
	uint8_t counter_active;
	// Offset added to counter
	uint64_t counter_offset;
	// Counter value when it was stopped
	uint64_t counter_when_stopped;
	// atomic registers
	std::atomic_uint32_t m_int_status;
	std::atomic_uint32_t m_int_enabled;
	// registers
	uint32_t multiplier, divider;
	uint32_t alarm;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PTIMER_INTR_0, "NV_PTIMER_INTR_0" },
		{ NV_PTIMER_INTR_EN_0, "NV_PTIMER_INTR_EN_0" },
		{ NV_PTIMER_NUMERATOR, "NV_PTIMER_NUMERATOR" },
		{ NV_PTIMER_DENOMINATOR, "NV_PTIMER_DENOMINATOR" },
		{ NV_PTIMER_TIME_0, "NV_PTIMER_TIME_0" },
		{ NV_PTIMER_TIME_1, "NV_PTIMER_TIME_1" },
		{ NV_PTIMER_ALARM_0, "NV_PTIMER_ALARM_0" }
	};
};

uint64_t
ptimer::Impl::counterToUs()
{
	// Tested on a Retail 1.0 xbox: the ratio is calculated with denominator / numerator, and not the other way around like it might seem at first. The gpu documentation
	// from envytools also indicates this. Also, the alarm value has no effect on the counter period, which is only affected by the ratio instead
	constexpr uint64_t max_alarm = 0xFFFFFFE0 >> 5;
	return util::muldiv128(util::muldiv128(max_alarm, timer::g_ticks_per_second, m_pramdac->getCoreFreq()), divider, multiplier);
}

uint64_t
ptimer::Impl::getNextAlarmTime(uint64_t now)
{
	if (((m_int_enabled & NV_PTIMER_INTR_EN_0_ALARM_ENABLED) | counter_active) == (NV_PTIMER_INTR_EN_0_ALARM_ENABLED | COUNTER_ON)) {
		uint64_t next_time, ptimer_period = counter_period;
		if (int64_t(now - last_alarm_time) >= ((int64_t)ptimer_period + counter_bias)) {
			counter_bias = 0;
			last_alarm_time = now;
			next_time = ptimer_period;

			m_int_status |= NV_PTIMER_INTR_0_ALARM_PENDING;
			m_pmc->updateIrq();
		}
		else {
			next_time = last_alarm_time + ptimer_period - now;
		}

		return next_time;
	}

	return std::numeric_limits<uint64_t>::max();
}

template<bool log, engine_enabled enabled>
void ptimer::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PTIMER_INTR_0:
		m_int_status &= ~value;
		m_pmc->updateIrq();
		break;

	case NV_PTIMER_INTR_EN_0:
		m_int_enabled = value;
		m_pmc->updateIrq();
		break;

	case NV_PTIMER_NUMERATOR:
		divider = value & NV_PTIMER_NUMERATOR_MASK;
		if (counter_active) {
			counter_period = counterToUs();
			cpu_set_timeout(m_lc86cpu, m_cpu->checkPeriodicEvents(timer::get_now()));
		}
		break;

	case NV_PTIMER_DENOMINATOR: {
		multiplier = value & NV_PTIMER_DENOMINATOR_MASK;
		if (multiplier > divider) [[unlikely]] {
			// Testing on a Retail 1.0 xbox shows that, when this condition is hit, the console hangs. We don't actually want to freeze the emulator, so
			// we will just terminate the emulation instead
			nxbx_fatal("Invalid ratio multiplier -> multiplier > divider (the real hardware would hang here)");
			break;
		}
		counter_active = multiplier ? COUNTER_ON : COUNTER_OFF; // A multiplier of zero stops the 56 bit counter
		uint64_t now = timer::get_now();
		if (counter_active) {
			counter_period = counterToUs();
			last_alarm_time = now;
		}
		else {
			counter_when_stopped = timer::get_dev_now(m_pramdac->getCoreFreq()) & 0x00FFFFFFFFFFFFFF;
		}
		cpu_set_timeout(m_lc86cpu, m_cpu->checkPeriodicEvents(now));
	}
	break;

		// Tested on a Retail 1.0 xbox: writing to the NV_PTIMER_TIME_0/1 registers causes the timer to start counting from the written value
	case NV_PTIMER_TIME_0:
		counter_offset = (counter_offset & (0xFFFFFFFFULL << 32)) | value;
		break;

	case NV_PTIMER_TIME_1:
		counter_offset = (counter_offset & 0xFFFFFFFFULL) | ((uint64_t)value << 32);
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
		alarm = value & ~0x1F; // tested on hw: writes of 1s to the first five bits have no impact
		uint32_t new_alarm = alarm >> 5;
		counter_bias = new_alarm - old_alarm;
		if (counter_active) {
			cpu_set_timeout(m_lc86cpu, m_cpu->checkPeriodicEvents(timer::get_now()));
		}
	}
	break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log, engine_enabled enabled>
uint32_t ptimer::Impl::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value = 0;

	switch (addr)
	{
	case NV_PTIMER_INTR_0:
		value = m_int_status;
		break;

	case NV_PTIMER_INTR_EN_0:
		value = m_int_enabled;
		break;

	case NV_PTIMER_NUMERATOR:
		value = divider;
		break;

	case NV_PTIMER_DENOMINATOR:
		value = multiplier;
		break;

	case NV_PTIMER_TIME_0: {
		// Returns the low 27 bits of the 56 bit counter
		uint64_t counter_base = counter_active ? timer::get_dev_now(m_pramdac->getCoreFreq()) : counter_when_stopped;
		value = uint32_t(((counter_offset + counter_base) & 0x7FFFFFF) << 5);
	}
	break;

	case NV_PTIMER_TIME_1: {
		// Returns the high 29 bits of the 56 bit counter
		uint64_t counter_base = counter_active ? timer::get_dev_now(m_pramdac->getCoreFreq()) : counter_when_stopped;
		value = uint32_t(((counter_offset + counter_base) >> 27) & 0x1FFFFFFF);
	}
	break;

	case NV_PTIMER_ALARM_0:
		value = alarm;
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool is_write>
auto ptimer::Impl::getIoFunc(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<ptimer::Impl, uint32_t, &ptimer::Impl::write32<true, on>, big> : nv2a_write<ptimer::Impl, uint32_t, &ptimer::Impl::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<ptimer::Impl, uint32_t, &ptimer::Impl::write32<false, on>, big> : nv2a_write<ptimer::Impl, uint32_t, &ptimer::Impl::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<ptimer::Impl, uint32_t, &ptimer::Impl::write32<false, off>, big>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<ptimer::Impl, uint32_t, &ptimer::Impl::read32<true, on>, big> : nv2a_read<ptimer::Impl, uint32_t, &ptimer::Impl::read32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_read<ptimer::Impl, uint32_t, &ptimer::Impl::read32<false, on>, big> : nv2a_read<ptimer::Impl, uint32_t, &ptimer::Impl::read32<false, on>, le>;
			}
		}
		else {
			return nv2a_read<ptimer::Impl, uint32_t, &ptimer::Impl::read32<false, off>, big>;
		}
	}
}

void ptimer::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_pmc->read32(NV_PMC_ENABLE) & NV_PMC_ENABLE_PTIMER;
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PTIMER_BASE, NV_PTIMER_SIZE, false,
		{
			.fnr32 = getIoFunc<false>(log, enabled, is_be),
			.fnw32 = getIoFunc<true>(log, enabled, is_be)
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update mmio region"));
	}
}

void
ptimer::Impl::reset()
{
	// Values dumped from a Retail 1.0 xbox
	m_int_status = NV_PTIMER_INTR_0_ALARM_NOT_PENDING;
	m_int_enabled = NV_PTIMER_INTR_EN_0_ALARM_DISABLED;
	multiplier = 0x00001DCD;
	divider = 0x0000DE86;
	alarm = 0xFFFFFFE0;
	counter_period = counterToUs();
	counter_active = COUNTER_ON;
	counter_offset = 0;
	counter_bias = 0;
	cpu_set_timeout(m_lc86cpu, m_cpu->checkPeriodicEvents(timer::get_now()));
}

void ptimer::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_pramdac = gpu->getPramdac();
	m_lc86cpu = cpu->get86cpu();
	m_cpu = cpu;
	reset();
	updateIo(false);
}

/** Public interface implementation **/
void ptimer::init(cpu *cpu, nv2a *gpu)
{
	m_impl->init(cpu, gpu);
}

void ptimer::reset()
{
	m_impl->reset();
}

void ptimer::updateIo()
{
	m_impl->updateIo();
}

uint32_t ptimer::read32(uint32_t addr)
{
	return m_impl->read32<false, on>(addr);
}

void ptimer::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false, on>(addr, value);
}

uint64_t ptimer::getNextAlarmTime(uint64_t now)
{
	return m_impl->getNextAlarmTime(now);
}

uint8_t ptimer::isCounterOn()
{
	return m_impl->isCounterOn();
}

void ptimer::setCounterPeriod(uint64_t new_period)
{
	m_impl->setCounterPeriod(new_period);
}

uint64_t ptimer::counterToUs()
{
	return m_impl->counterToUs();
}

ptimer::ptimer() : m_impl{std::make_unique<ptimer::Impl>()} {}
ptimer::~ptimer() {}
