// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pit.cpp

#include "machine.hpp"
#include "../clock.hpp"

#define MODULE_NAME pit

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

template<bool log>
void pit::write8(uint32_t addr, const uint8_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	uint8_t channel = addr & 3;

	switch (channel)
	{
	case 3: {
		channel = data >> 6;
		uint8_t opmode = data >> 1 & 7, bcd = data & 1, access = data >> 4 & 3;

		switch (channel)
		{
		case 3:
			nxbx_fatal("Read back command is not supported");
			break;

		case 0:
		case 1:
		case 2: {
			pit_channel *chan = &m_chan[channel];
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
		pit_channel *chan = &m_chan[channel];

		switch (chan->wmode)
		{
		case 0:
		case 1:
		case 2:
			nxbx_fatal("Read/Load mode must be LSB first MSB last");
			break;

		case 3:
			if (chan->lsb_read) {
				chan->counter = (static_cast<uint16_t>(data) << 8) | chan->counter;
				start_timer(channel);
				chan->lsb_read = 0;
			}
			else {
				chan->counter = data;
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

bool
pit::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x40, 4, true,
		{
		.fnw8 = log ? cpu_write<pit, uint8_t, &pit::write8<true>> : cpu_write<pit, uint8_t, &pit::write8<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update io ports");
		return false;
	}

	return true;
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
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
