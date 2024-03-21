// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pic.cpp

#include "machine.hpp"
#include <bit>

#define MODULE_NAME pic


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
				nxbx_fatal("Automatic rotation of IRQ priorities is not supported");
			}
		}
	}
	break;

	case 3: {
		if (value & 2) {
			read_isr = value & 1;
		}
		else if (value & 0x44) {
			nxbx_fatal("Unknown feature: %02X", value);
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
			nxbx_fatal("Configuration with no icw4 is not supported");
		}
		else if (value & 2) {
			nxbx_fatal("Single pic configuration is not supported");
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
			nxbx_fatal("MCS-80/85 mode is not supported");
		}
		else if (value & 2) {
			nxbx_fatal("Auto-eoi mode is not supported");
		}
		else if (value & 8) {
			nxbx_fatal("Buffered mode is not supported");
		}
		else if (value & 16) {
			nxbx_fatal("Special fully nested mode is not supported");
		}

		in_init = 0;
		icw_idx = 5;
		break;

	default:
		nxbx_fatal("Unknown icw specified, idx was %d", idx);
	}
}

template<bool log>
void pic::write(uint32_t addr, const uint8_t data)
{
	if constexpr (log) {
		log_io_write();
	}

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

template<bool log>
uint8_t pic::read(uint32_t addr)
{
	uint8_t value;

	if (addr & 1) {
		value = imr;
	}
	else {
		value = read_isr ? isr : irr;
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void pic::write_elcr(uint32_t addr, const uint8_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	elcr = data;
}

template<bool log>
uint8_t pic::read_elcr(uint32_t addr)
{
	uint8_t value = elcr;

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

bool
pic::update_io(bool is_update)
{
	bool log = module_enabled();
	if (idx == 0) {
		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x20, 2, true,
			{
				.fnr8 = log ? cpu_read<pic, uint8_t, &pic::read<true>> : cpu_read<pic, uint8_t, &pic::read<false>>,
				.fnw8 = log ? cpu_write<pic, uint8_t, &pic::write<true>> : cpu_write<pic, uint8_t, &pic::write<false>>
			},
			this, is_update, is_update))) {
			logger_en(error, "Failed to update io ports");
			return false;
		}

		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x4D0, 1, true,
			{
				.fnr8 = log ? cpu_read<pic, uint8_t, &pic::read_elcr<true>> : cpu_read<pic, uint8_t, &pic::read_elcr<false>>,
				.fnw8 = log ? cpu_write<pic, uint8_t, &pic::write_elcr<true>> : cpu_write<pic, uint8_t, &pic::write_elcr<false>>
			},
			this, is_update, is_update))) {
			logger_en(error, "Failed to update elcr io ports");
			return false;
		}
	}
	else {
		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0xA0, 2, true,
			{
				.fnr8 = log ? cpu_read<pic, uint8_t, &pic::read<true>> : cpu_read<pic, uint8_t, &pic::read<false>>,
				.fnw8 = log ? cpu_write<pic, uint8_t, &pic::write<true>> : cpu_write<pic, uint8_t, &pic::write<false>>
			},
			this, is_update, is_update))) {
			logger_en(error, "Failed to update pic io ports");
			return false;
		}

		if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0x4D1, 1, true,
			{
				.fnr8 = log ? cpu_read<pic, uint8_t, &pic::read_elcr<true>> : cpu_read<pic, uint8_t, &pic::read_elcr<false>>,
				.fnw8 = log ? cpu_write<pic, uint8_t, &pic::write_elcr<true>> : cpu_write<pic, uint8_t, &pic::write_elcr<false>>
			},
			this, is_update, is_update))) {
			logger_en(error, "Failed to update elcr io ports");
			return false;
		}
	}

	return true;
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
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
