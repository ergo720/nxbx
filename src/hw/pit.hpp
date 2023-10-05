// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <stdint.h>


struct pit_channel_t {
	uint8_t timer_mode, wmode;
	uint8_t timer_running, lsb_read;
	uint16_t counter;
	uint64_t last_irq_time;
};

struct pit_t {
	pit_channel_t chan[3];
};

inline pit_t pit;

void timer_init();
uint64_t get_now();
uint64_t pit_get_next_irq_time(uint64_t now);
void pit_write_handler(uint32_t port, const uint8_t value, void *opaque);
void pit_reset();
