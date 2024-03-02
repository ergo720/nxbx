// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pit.cpp

#include "machine.hpp"
#include "../clock.hpp"

#define PIT_IRQ_NUM 0


uint64_t
pit::counter_to_us()
{
	constexpr double time_scale = static_cast<double>(timer::ticks_per_second) / static_cast<double>(clock_freq);
	return (uint64_t)(static_cast<double>(m_chan[0].counter) * time_scale);
}

uint64_t
pit::get_next_irq_time(uint64_t now)
{
	if (m_chan[0].timer_running) {
		uint64_t next_time, pit_period = counter_to_us();
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

void
pit::start_timer(uint8_t channel)
{
	m_chan[channel].last_irq_time = timer::get_now();
	m_chan[channel].timer_running = 1;
	cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(m_chan[channel].last_irq_time));
}

void
pit::write_handler(uint32_t port, const uint8_t value)
{
	uint8_t channel = port & 3;

	switch (channel)
	{
	case 3: {
		channel = value >> 6;
		uint8_t opmode = value >> 1 & 7, bcd = value & 1, access = value >> 4 & 3;

		switch (channel)
		{
		case 3:
			nxbx::fatal("Read back command is not supported");
			break;

		case 0:
		case 1:
		case 2: {
			pit_channel *chan = &m_chan[channel];
			if (!access) {
				nxbx::fatal("Counter latch command is not supported");
			}
			else {
				if (bcd) {
					nxbx::fatal("BCD mode not supported");
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
		pit_channel *chan = &m_chan[channel];

		switch (chan->wmode)
		{
		case 0:
		case 1:
		case 2:
			nxbx::fatal("Read/Load mode must be LSB first MSB last");
			break;

		case 3:
			if (chan->lsb_read) {
				chan->counter = (static_cast<uint16_t>(value) << 8) | chan->counter;
				start_timer(channel);
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

void
pit::channel_reset(uint8_t channel)
{
	m_chan[channel].counter = 0;
	m_chan[channel].timer_mode = 0;
	m_chan[channel].lsb_read = 0;
	m_chan[channel].timer_running = 0;
}

void
pit::reset()
{
	for (unsigned i = 0; i < 3; ++i) {
		channel_reset(i);
	}
}

bool
pit::init()
{
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x40, 4, true,
		{
		.fnw8 = cpu_write<pit, uint8_t, &pit::write_handler>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s io ports", get_name());
		return false;
	}

	reset();
	return true;
}
