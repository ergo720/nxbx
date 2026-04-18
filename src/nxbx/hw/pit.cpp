// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720
// SPDX-FileCopyrightText: 2020 Halfix devs
// This code is derived from https://github.com/nepx/halfix/blob/master/src/hardware/pit.c

#include "lib86cpu.hpp"
#include "machine.hpp"
#include "cpu.hpp"
#include "pit.hpp"
#include "clock.hpp"

#define MODULE_NAME pit

#define PIT_IRQ_NUM 0
#define PIT_CHANNEL0_DATA  0x40
#define PIT_PORT_CMD       0x43


struct PitChannel
{
	uint8_t timer_mode, wmode;
	uint8_t timer_running, lsb_read;
	uint16_t counter;
	uint64_t last_irq_time;
};

/** Private device implementation **/
class pit::Impl
{
public:
	void init(machine *machine);
	void reset();
	void updateIoLogging() { updateIo(true); }
	uint64_t getNextIrqTime(uint64_t now);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);

private:
	void updateIo(bool is_update);
	uint64_t counterToUs();
	void startTimer(uint8_t channel);
	void channelReset(uint8_t channel);

	PitChannel m_chan[3];
	// NOTE: on the xbox, the pit frequency is 6% lower than the default one, see https://xboxdevwiki.net/Porting_an_Operating_System_to_the_Xbox_HOWTO#Timer_Frequency
	static constexpr uint64_t clock_freq = 1125000;
	// connected devices
	machine *m_machine;
	cpu *m_cpu;
	cpu_t *m_lc86cpu;
	// registers
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ PIT_CHANNEL0_DATA, "CHANNEL0_DATA" },
		{ PIT_PORT_CMD, "COMMAND" },
	};
};

uint64_t pit::Impl::counterToUs()
{
	constexpr double time_scale = static_cast<double>(timer::g_ticks_per_second) / static_cast<double>(clock_freq);
	return (uint64_t)(static_cast<double>(m_chan[0].counter) * time_scale);
}

uint64_t pit::Impl::getNextIrqTime(uint64_t now)
{
	if (m_chan[0].timer_running) {
		uint64_t next_time, pit_period = counterToUs();
		if (now - m_chan[0].last_irq_time >= pit_period) {
			m_chan[0].last_irq_time = now;
			next_time = pit_period;

			m_machine->lower_irq(PIT_IRQ_NUM);
			m_machine->raise_irq(PIT_IRQ_NUM);
		}
		else {
			next_time = m_chan[0].last_irq_time + pit_period - now;
		}

		return next_time;
	}

	return std::numeric_limits<uint64_t>::max();
}

void pit::Impl::startTimer(uint8_t channel)
{
	m_chan[channel].last_irq_time = timer::get_now();
	m_chan[channel].timer_running = 1;
	cpu_set_timeout(m_lc86cpu, m_cpu->checkPeriodicEvents(m_chan[channel].last_irq_time));
}

template<bool log>
void pit::Impl::write8(uint32_t addr, const uint8_t value)
{
	if constexpr (log) {
		log_io_write();
	}

	uint8_t channel = addr & 3;

	switch (channel)
	{
	case 3: {
		channel = value >> 6;
		uint8_t opmode = value >> 1 & 7, bcd = value & 1, access = value >> 4 & 3;

		switch (channel)
		{
		case 3:
			nxbx_fatal("Read back command is not supported");
			break;

		case 0:
		case 1:
		case 2: {
			PitChannel *chan = &m_chan[channel];
			if (!access) {
				nxbx_fatal("Counter latch command is not supported");
			}
			else {
				if (bcd) {
					nxbx_fatal("BCD mode not supported");
				}

				chan->wmode = access;
				chan->timer_mode = opmode;
				if ((chan->timer_mode == 2) && (channel == 0)) {
					m_machine->raise_irq(PIT_IRQ_NUM);
				}
			}
			break;
		}
		}
		break;
	}
	break;

	case 0:
	case 1:
	case 2: {
		PitChannel *chan = &m_chan[channel];

		switch (chan->wmode)
		{
		case 0:
		case 1:
		case 2:
			nxbx_fatal("Read/Load mode must be LSB first MSB last");
			break;

		case 3:
			if (chan->lsb_read) {
				chan->counter = (static_cast<uint16_t>(value) << 8) | chan->counter;
				startTimer(channel);
				chan->lsb_read = 0;
			}
			else {
				chan->counter = value;
				chan->lsb_read = 1;
			}
			break;
		}
		break;
	}
	}
}

void pit::Impl::channelReset(uint8_t channel)
{
	m_chan[channel].counter = 0;
	m_chan[channel].timer_mode = 0;
	m_chan[channel].lsb_read = 0;
	m_chan[channel].timer_running = 0;
}

void pit::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, 0x40, 4, true,
		{
		.fnw8 = log ? cpu_write<pit::Impl, uint8_t, &pit::Impl::write8<true>> : cpu_write<pit::Impl, uint8_t, &pit::Impl::write8<false>>
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update io ports"));
	}
}

void pit::Impl::reset()
{
	for (unsigned i = 0; i < 3; ++i) {
		channelReset(i);
	}
}

void pit::Impl::init(machine *machine)
{
	m_lc86cpu = machine->get86cpu();
	m_cpu = machine->getCpu();
	m_machine = machine;
	updateIo(false);
	reset();
}

/** Public interface implementation **/
void pit::init(machine *machine)
{
	m_impl->init(machine);
}

void pit::reset()
{
	m_impl->reset();
}

void pit::updateIoLogging()
{
	m_impl->updateIoLogging();
}

uint64_t pit::getNextIrqTime(uint64_t now)
{
	return m_impl->getNextIrqTime(now);
}

pit::pit() : m_impl{std::make_unique<pit::Impl>()} {}
pit::~pit() {}
