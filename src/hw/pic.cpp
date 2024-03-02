// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pic.cpp

#include "machine.hpp"
#include <bit>


uint8_t
pic::get_interrupt()
{
	uint8_t irq = highest_priority_irq_to_send, irq_mask = 1 << irq;
	if ((irr & irq_mask) == 0) {
		// Generate a spurious IRQ7 if the interrupt is no longer pending
		return vector_offset | 7;
	}

	// If edge triggered, then clear irr
	if ((elcr & irq_mask) == 0) {
		irr &= ~irq_mask;
	}

	isr |= irq_mask;

	if (is_master() && irq == 2) {
		return m_machine->get<pic, 1>().get_interrupt();
	}

	return vector_offset + irq;
}

uint8_t
pic::get_interrupt_for_cpu()
{
	cpu_lower_hw_int_line(m_machine->get<cpu_t *>());
	return get_interrupt();
}

uint16_t
get_interrupt_for_cpu(void *opaque)
{
	pic *master = static_cast<pic *>(opaque);
	return master->get_interrupt_for_cpu();
}

void
pic::update_state()
{
	uint8_t unmasked, isr1;

	if (!(unmasked = irr & ~imr)) {
		// All interrupts masked, nothing to do
		return;
	}

	// Left rotate IRR and ISR so that the interrupts are located in decreasing priority
	unmasked = std::rotl(unmasked, priority_base ^ 7);
	isr1 = std::rotl(isr, priority_base ^ 7);

	for (unsigned i = 0; i < 8; ++i) {
		uint8_t mask = 1 << i;
		if (isr1 & mask) {
			return;
		}

		if (unmasked & (1 << i)) {
			highest_priority_irq_to_send = (priority_base + 1 + i) & 7;

			if (is_master()) {
				cpu_raise_hw_int_line(m_machine->get<cpu_t *>());
			}
			else {
				m_machine->lower_irq(2);
				m_machine->raise_irq(2);
			}

			return;
		}
	}
}

void
pic::raise_irq(uint8_t irq)
{
	uint8_t mask = 1 << irq;

	if (elcr & mask) {
		// level triggered
		pin_state |= mask;
		irr |= mask;
		update_state();
	}
	else {
		// edge triggered
		if ((pin_state & mask) == 0) {
			pin_state |= mask;
			irr |= mask;
			update_state();
		}
		else {
			pin_state |= mask;
		}
	}
}

void
pic::lower_irq(uint8_t irq)
{
	uint8_t mask = 1 << irq;
	pin_state &= ~mask;
	irr &= ~mask;

	if (!is_master() && !irr) {
		m_machine->lower_irq(2);
	}
}

void
pic::write_ocw(unsigned idx, uint8_t value)
{
	switch (idx)
	{
	case 1:
		imr = value;
		update_state();
		break;

	case 2: {
		uint8_t rotate = value & 0x80, specific = value & 0x40, eoi = value & 0x20, irq = value & 7;
		if (eoi) {
			if (specific) {
				isr &= ~(1 << irq);
				if (rotate) {
					priority_base = irq;
				}
			}
			else {
				// Clear the highest priority irq
				uint8_t highest = (priority_base + 1) & 7;
				for (unsigned i = 0; i < 8; ++i) {
					uint8_t mask = 1 << ((highest + i) & 7);
					if (isr & mask) {
						isr &= ~mask;
						break;
					}
				}
				if (rotate) {
					priority_base = irq;
				}
			}
			update_state();
		}
		else {
			if (specific) {
				if (rotate) {
					priority_base = irq;
				}
			}
			else {
				nxbx::fatal("Automatic rotation of IRQ priorities is not supported");
			}
		}
	}
	break;

	case 3: {
		if (value & 2) {
			read_isr = value & 1;
		}
		else if (value & 0x44) {
			nxbx::fatal("Unknown feature: %02X", value);
		}
	}
	}
}

void
pic::write_icw(unsigned idx, uint8_t value)
{
	switch (idx)
	{
	case 1:
		if ((value & 1) == 0) {
			nxbx::fatal("Configuration with no icw4 is not supported");
		}
		else if (value & 2) {
			nxbx::fatal("Single pic configuration is not supported");
		}

		in_init = 1;
		imr = 0;
		isr = 0;
		irr = 0;
		priority_base = 7;
		icw_idx = 2;
		break;

	case 2:
		vector_offset = value & ~7;
		icw_idx = 3;
		break;

	case 3:
		icw_idx = 4;
		break;

	case 4:
		if ((value & 1) == 0) {
			nxbx::fatal("MCS-80/85 mode is not supported");
		}
		else if (value & 2) {
			nxbx::fatal("Auto-eoi mode is not supported");
		}
		else if (value & 8) {
			nxbx::fatal("Buffered mode is not supported");
		}
		else if (value & 16) {
			nxbx::fatal("Special fully nested mode is not supported");
		}

		in_init = 0;
		icw_idx = 5;
		break;

	default:
		nxbx::fatal("Unknown icw specified, idx was %d", idx);
	}
}

void
pic::write_handler(uint32_t addr, const uint8_t data)
{
	if ((addr & 1) == 0) {
		switch (data >> 3 & 3)
		{
		case 0:
			write_ocw(2, data);
			break;

		case 1:
			write_ocw(3, data);
			break;

		default:
			cpu_lower_hw_int_line(m_machine->get<cpu_t *>());
			write_icw(1, data);
		}
	}
	else {
		if (in_init) {
			write_icw(icw_idx, data);
		}
		else {
			write_ocw(1, data);
		}
	}
}

uint8_t
pic::read_handler(uint32_t addr)
{
	if (addr & 1) {
		return imr;
	}
	else {
		return read_isr ? isr : irr;
	}
}

void
pic::elcr_write_handler(uint32_t addr, const uint8_t data)
{
	elcr = data;
}

uint8_t
pic::elcr_read_handler(uint32_t addr)
{
	return elcr;
}

void
pic::reset()
{
	vector_offset = 0;
	imr = 0xFF;
	irr = 0;
	isr = 0;
	in_init = 0;
	read_isr = 0;
}

bool
pic::init()
{
	if (idx == 0) {
		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x20, 2, true,
			{
				.fnr8 = cpu_read<pic, uint8_t, &pic::read_handler>,
				.fnw8 = cpu_write<pic, uint8_t, &pic::write_handler>
			},
			this))) {
			logger(log_lv::error, "Failed to initialize %s io ports", get_name());
			return false;
		}

		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x4D0, 1, true,
			{
				.fnr8 = cpu_read<pic, uint8_t, &pic::elcr_read_handler>,
				.fnw8 = cpu_write<pic, uint8_t, &pic::elcr_write_handler>
			},
			this))) {
			logger(log_lv::error, "Failed to initialize %s elcr io ports", get_name());
			return false;
		}
	}
	else {
		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0xA0, 2, true,
			{
				.fnr8 = cpu_read<pic, uint8_t, &pic::read_handler>,
				.fnw8 = cpu_write<pic, uint8_t, &pic::write_handler>
			},
			this))) {
			logger(log_lv::error, "Failed to initialize slave pic io ports", get_name());
			return false;
		}

		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x4D1, 1, true,
			{
				.fnr8 = cpu_read<pic, uint8_t, &pic::elcr_read_handler>,
				.fnw8 = cpu_write<pic, uint8_t, &pic::elcr_write_handler>
			},
			this))) {
			logger(log_lv::error, "Failed to initialize %s elcr io ports", get_name());
			return false;
		}
	}

	reset();
	return true;
}
