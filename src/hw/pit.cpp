// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pit.cpp

#include "pit.hpp"
#include "pic.hpp"
#include "cpu.hpp"
#include "../clock.hpp"
#include "../init.hpp"


// NOTE: on the xbox, the pit frequency is 6% lower than the default one, see https://xboxdevwiki.net/Porting_an_Operating_System_to_the_Xbox_HOWTO#Timer_Frequency
constexpr uint64_t pit_clock_freq = 1125000;

static inline uint64_t
pit_counter_to_us()
{
	constexpr double time_scale = static_cast<double>(ticks_per_second) / static_cast<double>(pit_clock_freq);
	return (uint64_t)(static_cast<double>(pit.chan[0].counter) * time_scale);
}

uint64_t
pit_get_next_irq_time(uint64_t now)
{
	if (pit.chan[0].timer_running) {
		uint64_t next_time, pit_period = pit_counter_to_us();
		if (now - pit.chan[0].last_irq_time >= pit_period) {
			pit.chan[0].last_irq_time = now;
			next_time = pit_period;

			pic_lower_irq(0);
			pic_raise_irq(0);
		}
		else {
			next_time = pit.chan[0].last_irq_time + pit_period - now;
		}

		return next_time;
	}

	return std::numeric_limits<uint64_t>::max();
}

static void
pit_start_timer(pit_channel_t *chan)
{
	chan->last_irq_time = get_now();
	chan->timer_running = 1;
	cpu_set_timeout(g_cpu, pit_get_next_irq_time(chan->last_irq_time));
}

void
pit_write_handler(uint32_t port, const uint8_t value, void *opaque)
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
			nxbx_fatal("Read back command is not supported");
			break;

		case 0:
		case 1:
		case 2: {
			pit_channel_t *chan = &pit.chan[channel];
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
					pic_raise_irq(0);
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
		pit_channel_t *chan = &pit.chan[channel];

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
				pit_start_timer(chan);
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

static void
pit_channel_reset(pit_channel_t *chan)
{
	chan->counter = 0;
	chan->timer_mode = 0;
	chan->lsb_read = 0;
	chan->timer_running = 0;
}

static void
pit_reset()
{
	for (pit_channel_t &chan : pit.chan) {
		pit_channel_reset(&chan);
	}
}

void
pit_init()
{
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, 0x40, 4, true, io_handlers_t{ .fnw8 = pit_write_handler }, nullptr))) {
		throw nxbx_exp_abort("Failed to initialize pit I/O ports");
	}

	add_reset_func(pit_reset);
	pit_reset();
}
