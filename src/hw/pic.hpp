// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <stdint.h>


struct pic_t {
	uint8_t imr, irr, isr, elcr;
	uint8_t read_isr, in_init;
	uint8_t vector_offset;
	uint8_t priority_base;
	uint8_t highest_priority_irq_to_send;
	uint8_t pin_state;
	unsigned icw_idx;
};

// 0: master, 1: slave
inline pic_t g_pic[2];

uint8_t pic_read_handler(uint32_t port, void *opaque);
void pic_write_handler(uint32_t addr, const uint8_t data, void *opaque);
uint8_t pic_elcr_read_handler(uint32_t addr, void *opaque);
void pic_elcr_write_handler(uint32_t addr, const uint8_t data, void *opaque);
uint16_t pic_get_interrupt();
void pic_raise_irq(uint8_t a);
void pic_lower_irq(uint8_t a);
void pic_reset();
