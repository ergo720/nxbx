// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <stdint.h>
#include <string_view>


class machine;

class pic {
public:
	pic(machine *machine, unsigned idx, const char *const name) : m_machine(machine), m_name(name), idx(idx) {}
	bool init();
	void reset();
	constexpr std::string_view get_name() { return m_name; }
	uint8_t get_interrupt_for_cpu();
	void raise_irq(uint8_t a);
	void lower_irq(uint8_t a);
	uint8_t read_handler(uint32_t addr);
	void write_handler(uint32_t addr, const uint8_t data);
	uint8_t elcr_read_handler(uint32_t addr);
	void elcr_write_handler(uint32_t addr, const uint8_t data);

private:
	bool is_master() { return idx == 0; }
	uint8_t get_interrupt();
	void update_state();
	void write_ocw(unsigned idx, uint8_t value);
	void write_icw(unsigned idx, uint8_t value);

	machine *const m_machine;
	const char *const m_name;
	uint8_t imr, irr, isr, elcr;
	uint8_t read_isr, in_init;
	uint8_t vector_offset;
	uint8_t priority_base;
	uint8_t highest_priority_irq_to_send;
	uint8_t pin_state;
	unsigned icw_idx;
	unsigned idx; // 0: master, 1: slave
};

uint16_t get_interrupt_for_cpu(void *opaque);
