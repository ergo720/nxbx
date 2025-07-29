// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <stdint.h>


class machine;

struct pit_channel {
	uint8_t timer_mode, wmode;
	uint8_t timer_running, lsb_read;
	uint16_t counter;
	uint64_t last_irq_time;
};

class pit {
public:
	pit(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	uint64_t get_next_irq_time(uint64_t now);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);

private:
	bool update_io(bool is_update);
	uint64_t counter_to_us();
	void start_timer(uint8_t channel);
	void channel_reset(uint8_t channel);

	machine *const m_machine;
	pit_channel m_chan[3];
	// NOTE: on the xbox, the pit frequency is 6% lower than the default one, see https://xboxdevwiki.net/Porting_an_Operating_System_to_the_Xbox_HOWTO#Timer_Frequency
	static constexpr uint64_t clock_freq = 1125000;
};
