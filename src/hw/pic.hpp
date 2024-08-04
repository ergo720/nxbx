// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <stdint.h>


class machine;

class pic {
public:
	pic(machine *machine, unsigned idx, const char *const name) : m_machine(machine), m_name(name), idx(idx) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	uint8_t get_interrupt_for_cpu();
	void raise_irq(uint8_t a);
	void lower_irq(uint8_t a);
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t data);
	template<bool log = false>
	uint8_t read8_elcr(uint32_t addr);
	template<bool log = false>
	void write8_elcr(uint32_t addr, const uint8_t data);

private:
	bool update_io(bool is_update);
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
