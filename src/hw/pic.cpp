// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pic.cpp

#include "pic.hpp"
#include "cpu.hpp"
#include "../init.hpp"
#include <bit>


static inline bool
pic_is_master(pic_t *pic)
{
	return pic == &g_pic[0];
}

static uint8_t
pic_get_interrupt(pic_t *pic)
{
	uint8_t irq = pic->highest_priority_irq_to_send, irq_mask = 1 << irq;
	if ((pic->irr & irq_mask) == 0) {
		// Generate a spurious IRQ7 if the interrupt is no longer pending
		return pic->vector_offset | 7;
	}

	// If edge triggered, then clear irr
	if ((pic->elcr & irq_mask) == 0) {
		pic->irr &= ~irq_mask;
	}

	pic->isr |= irq_mask;

	if (pic_is_master(pic) && irq == 2) {
		return pic_get_interrupt(&g_pic[1]);
	}

	return pic->vector_offset + irq;
}

uint16_t
pic_get_interrupt()
{
	cpu_lower_hw_int_line(g_cpu);
	return pic_get_interrupt(&g_pic[0]);
}

static void
pic_update_state(pic_t *pic)
{
	uint8_t unmasked, isr;

	if (!(unmasked = pic->irr & ~pic->imr)) {
		// All interrupts masked, nothing to do
		return;
	}

	// Left rotate IRR and ISR so that the interrupts are located in decreasing priority
	unmasked = std::rotl(unmasked, pic->priority_base ^ 7);
	isr = std::rotl(pic->isr, pic->priority_base ^ 7);

	for (unsigned i = 0; i < 8; ++i) {
		uint8_t mask = 1 << i;
		if (isr & mask) {
			return;
		}

		if (unmasked & (1 << i)) {
			pic->highest_priority_irq_to_send = (pic->priority_base + 1 + i) & 7;

			if (pic_is_master(pic)) {
				cpu_raise_hw_int_line(g_cpu);
			}
			else {
				pic_lower_irq(2);
				pic_raise_irq(2);
			}

			return;
		}
	}
}

static void
pic_raise_irq(pic_t *pic, uint8_t irq)
{
	uint8_t mask = 1 << irq;

	if (pic->elcr & mask) {
		// level triggered
		pic->pin_state |= mask;
		pic->irr |= mask;
		pic_update_state(pic);
	}
	else {
		// edge triggered
		if ((pic->pin_state & mask) == 0) {
			pic->pin_state |= mask;
			pic->irr |= mask;
			pic_update_state(pic);
		}
		else {
			pic->pin_state |= mask;
		}
	}
}

static void
pic_lower_irq(pic_t *pic, uint8_t irq)
{
	uint8_t mask = 1 << irq;
	pic->pin_state &= ~mask;
	pic->irr &= ~mask;

	if (!pic_is_master(pic) && !pic->irr) {
		pic_lower_irq(2);
	}
}

void
pic_raise_irq(uint8_t a)
{
	pic_raise_irq(&g_pic[a > 7 ? 1 : 0], a & 7);
}

void
pic_lower_irq(uint8_t a)
{
	pic_lower_irq(&g_pic[a > 7 ? 1 : 0], a & 7);
}

static void
pic_write_ocw(pic_t *pic, unsigned idx, uint8_t value)
{
	switch (idx)
	{
	case 1:
		pic->imr = value;
		pic_update_state(pic);
		break;

	case 2: {
		uint8_t rotate = value & 0x80, specific = value & 0x40, eoi = value & 0x20, irq = value & 7;
		if (eoi) {
			if (specific) {
				pic->isr &= ~(1 << irq);
				if (rotate) {
					pic->priority_base = irq;
				}
			}
			else {
				// Clear the highest priority irq
				uint8_t highest = (pic->priority_base + 1) & 7;
				for (unsigned i = 0; i < 8; ++i) {
					uint8_t mask = 1 << ((highest + i) & 7);
					if (pic->isr & mask) {
						pic->isr &= ~mask;
						break;
					}
				}
				if (rotate) {
					pic->priority_base = irq;
				}
			}
			pic_update_state(pic);
		}
		else {
			if (specific) {
				if (rotate) {
					pic->priority_base = irq;
				}
			}
			else {
				logger(log_lv::error, "Automatic rotation of IRQ priorities is not supported");
				cpu_exit(g_cpu);
			}
		}
	}
	break;

	case 3: {
		if (value & 2) {
			pic->read_isr = value & 1;
		}
		else if (value & 0x44) {
			logger(log_lv::error, "Unknown feature: %02X", value);
			cpu_exit(g_cpu);
		}
	}
	}
}

static void
pic_write_icw(pic_t *pic, unsigned idx, uint8_t value)
{
	switch (idx)
	{
	case 1:
		if ((value & 1) == 0) {
			logger(log_lv::error, "Configuration with no icw4 is not supported");
			cpu_exit(g_cpu);
		}
		else if (value & 2) {
			logger(log_lv::error, "Single pic configuration is not supported");
			cpu_exit(g_cpu);
		}

		pic->in_init = 1;
		pic->imr = 0;
		pic->isr = 0;
		pic->irr = 0;
		pic->priority_base = 7;
		pic->icw_idx = 2;
		break;

	case 2:
		pic->vector_offset = value & ~7;
		pic->icw_idx = 3;
		break;

	case 3:
		pic->icw_idx = 4;
		break;

	case 4:
		if ((value & 1) == 0) {
			logger(log_lv::error, "MCS-80/85 mode is not supported");
			cpu_exit(g_cpu);
		}
		else if (value & 2) {
			logger(log_lv::error, "Auto-eoi mode is not supported");
			cpu_exit(g_cpu);
		}
		else if (value & 8) {
			logger(log_lv::error, "Buffered mode is not supported");
			cpu_exit(g_cpu);
		}
		else if (value & 16) {
			logger(log_lv::error, "Special fully nested mode is not supported");
			cpu_exit(g_cpu);
		}

		pic->in_init = 0;
		pic->icw_idx = 5;
		break;

	default:
		logger(log_lv::error, "Unknown icw specified, idx was %d", idx);
		cpu_exit(g_cpu);
	}
}

void
pic_write_handler(uint32_t addr, const uint8_t data, void *opaque)
{
	pic_t *pic = static_cast<pic_t *>(opaque);
	if ((addr & 1) == 0) {
		switch (data >> 3 & 3)
		{
		case 0:
			pic_write_ocw(pic, 2, data);
			break;

		case 1:
			pic_write_ocw(pic, 3, data);
			break;

		default:
			cpu_lower_hw_int_line(g_cpu);
			pic_write_icw(pic, 1, data);
		}
	}
	else {
		if (pic->in_init) {
			pic_write_icw(pic, pic->icw_idx, data);
		}
		else {
			pic_write_ocw(pic, 1, data);
		}
	}
}

uint8_t
pic_read_handler(uint32_t port, void *opaque)
{
	pic_t *pic = static_cast<pic_t *>(opaque);
	if (port & 1) {
		return pic->imr;
	}
	else {
		return pic->read_isr ? pic->isr : pic->irr;
	}
}

void
pic_elcr_write_handler(uint32_t addr, const uint8_t data, void *opaque)
{
	static_cast<pic_t *>(opaque)[addr & 1].elcr = data;
}

uint8_t
pic_elcr_read_handler(uint32_t addr, void *opaque)
{
	return static_cast<pic_t *>(opaque)[addr & 1].elcr;
}

static void
pic_reset()
{
	for (auto &pic : g_pic) {
		pic.vector_offset = 0;
		pic.imr = 0xFF;
		pic.irr = 0;
		pic.isr = 0;
		pic.in_init = 0;
		pic.read_isr = 0;
	}
}

void
pic_init()
{
	add_reset_func(pic_reset);
	pic_reset();
}
