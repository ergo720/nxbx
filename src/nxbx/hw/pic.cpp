// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720
// SPDX-FileCopyrightText: 2020 Halfix devs
// This code is derived from https://github.com/nepx/halfix/blob/master/src/hardware/pic.c

#include "lib86cpu.hpp"
#include "machine.hpp"
#include "pic.hpp"
#include "cpu.hpp"
#include <mutex>
#include <bit>

#define MODULE_NAME pic

#define PIC_MASTER_CMD          0x20
#define PIC_MASTER_DATA         0x21
#define PIC_MASTER_ELCR         0x4D0
#define PIC_SLAVE_CMD           0xA0
#define PIC_SLAVE_DATA          0xA1
#define PIC_SLAVE_ELCR          0x4D1


/** Private device implementation **/
class pic::Impl
{
public:
	void init(machine *machine, unsigned idx);
	void reset();
	void updateIoLogging() { updateIo(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);
	template<bool log = false>
	uint8_t read8elcr(uint32_t addr);
	template<bool log = false>
	void write8elcr(uint32_t addr, const uint8_t value);
	static uint16_t getInterruptForCpu(void *opaque);
	void raiseIrq(uint8_t a);
	void lowerIrq(uint8_t a);

	static inline std::mutex m_mtx;

private:
	void updateIo(bool is_update);
	bool isMaster() { return m_idx == 0; }
	void updateState();
	void writeOcw(unsigned idx, uint8_t value);
	void writeIcw(unsigned idx, uint8_t value);
	uint8_t lowerIntLineAndGetInterrupt();
	uint8_t getInterrupt();

	uint8_t imr, irr, isr, elcr;
	uint8_t read_isr, in_init;
	uint8_t vector_offset;
	uint8_t priority_base;
	uint8_t highest_priority_irq_to_send;
	uint8_t pin_state;
	unsigned icw_idx;
	unsigned m_idx; // 0: master, 1: slave
	// connected devices
	static inline pic::Impl *m_picImpl[2];
	cpu_t *m_lc86cpu;
	// registers
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ PIC_MASTER_CMD, "MASTER_COMMAND" },
		{ PIC_MASTER_DATA, "MASTER_DATA" },
		{ PIC_MASTER_ELCR, "MASTER_ELCR" },
		{ PIC_SLAVE_CMD, "SLAVE_COMMAND" },
		{ PIC_SLAVE_DATA, "SLAVE_DATA" },
		{ PIC_SLAVE_ELCR, "SLAVE_ELCR" },
	};
};

uint8_t pic::Impl::getInterrupt()
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

	if (isMaster() && irq == 2) {
		return m_picImpl[1]->getInterrupt();
	}

	return vector_offset + irq;
}

uint8_t pic::Impl::lowerIntLineAndGetInterrupt()
{
	cpu_lower_hw_int_line(m_lc86cpu);
	return getInterrupt();
}

uint16_t pic::Impl::getInterruptForCpu(void *opaque)
{
	// NOTE: called from the cpu thread when it services a hw interrupt
	std::unique_lock lock(pic::Impl::m_mtx);
	pic::Impl *master = static_cast<pic::Impl *>(opaque);
	return master->lowerIntLineAndGetInterrupt();
}

void
pic::Impl::updateState()
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

			if (isMaster()) {
				cpu_raise_hw_int_line(m_lc86cpu);
			}
			else {
				m_picImpl[0]->lowerIrq(2);
				m_picImpl[0]->raiseIrq(2);
			}

			return;
		}
	}
}

void
pic::Impl::raiseIrq(uint8_t irq)
{
	uint8_t mask = 1 << irq;

	if (elcr & mask) {
		// level triggered
		pin_state |= mask;
		irr |= mask;
		updateState();
	}
	else {
		// edge triggered
		if ((pin_state & mask) == 0) {
			pin_state |= mask;
			irr |= mask;
			updateState();
		}
		else {
			pin_state |= mask;
		}
	}
}

void
pic::Impl::lowerIrq(uint8_t irq)
{
	uint8_t mask = 1 << irq;
	pin_state &= ~mask;
	irr &= ~mask;

	if (!isMaster() && !irr) {
		m_picImpl[0]->lowerIrq(2);
	}
}

void
pic::Impl::writeOcw(unsigned idx, uint8_t value)
{
	switch (idx)
	{
	case 1:
		imr = value;
		updateState();
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
			updateState();
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
pic::Impl::writeIcw(unsigned idx, uint8_t value)
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
void pic::Impl::write8(uint32_t addr, const uint8_t value)
{
	std::unique_lock lock(pic::Impl::m_mtx);
	if constexpr (log) {
		log_io_write();
	}

	if ((addr & 1) == 0) {
		switch (value >> 3 & 3)
		{
		case 0:
			writeOcw(2, value);
			break;

		case 1:
			writeOcw(3, value);
			break;

		default:
			cpu_lower_hw_int_line(m_lc86cpu);
			writeIcw(1, value);
		}
	}
	else {
		if (in_init) {
			writeIcw(icw_idx, value);
		}
		else {
			writeOcw(1, value);
		}
	}
}

template<bool log>
uint8_t pic::Impl::read8(uint32_t addr)
{
	std::unique_lock lock(pic::Impl::m_mtx);
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
void pic::Impl::write8elcr(uint32_t addr, const uint8_t value)
{
	std::unique_lock lock(pic::Impl::m_mtx);
	if constexpr (log) {
		log_io_write();
	}

	elcr = value;
}

template<bool log>
uint8_t pic::Impl::read8elcr(uint32_t addr)
{
	std::unique_lock lock(pic::Impl::m_mtx);
	uint8_t value = elcr;

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

void pic::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	if (m_idx == 0) {
		if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, 0x20, 2, true,
			{
				.fnr8 = log ? cpu_read<pic::Impl, uint8_t, &pic::Impl::read8<true>> : cpu_read<pic::Impl, uint8_t, &pic::Impl::read8<false>>,
				.fnw8 = log ? cpu_write<pic::Impl, uint8_t, &pic::Impl::write8<true>> : cpu_write<pic::Impl, uint8_t, &pic::Impl::write8<false>>
			},
			this, is_update, is_update))) {
			throw std::runtime_error(lv2str(highest, "Failed to update io ports"));
		}

		if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, 0x4D0, 1, true,
			{
				.fnr8 = log ? cpu_read<pic::Impl, uint8_t, &pic::Impl::read8elcr<true>> : cpu_read<pic::Impl, uint8_t, &pic::Impl::read8elcr<false>>,
				.fnw8 = log ? cpu_write<pic::Impl, uint8_t, &pic::Impl::write8elcr<true>> : cpu_write<pic::Impl, uint8_t, &pic::Impl::write8elcr<false>>
			},
			this, is_update, is_update))) {
			throw std::runtime_error(lv2str(highest, "Failed to update elcr io ports"));
		}
	}
	else {
		if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, 0xA0, 2, true,
			{
				.fnr8 = log ? cpu_read<pic::Impl, uint8_t, &pic::Impl::read8<true>> : cpu_read<pic::Impl, uint8_t, &pic::Impl::read8<false>>,
				.fnw8 = log ? cpu_write<pic::Impl, uint8_t, &pic::Impl::write8<true>> : cpu_write<pic::Impl, uint8_t, &pic::Impl::write8<false>>
			},
			this, is_update, is_update))) {
			throw std::runtime_error(lv2str(highest, "Failed to update pic::Impl io ports"));
		}

		if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, 0x4D1, 1, true,
			{
				.fnr8 = log ? cpu_read<pic::Impl, uint8_t, &pic::Impl::read8elcr<true>> : cpu_read<pic::Impl, uint8_t, &pic::Impl::read8elcr<false>>,
				.fnw8 = log ? cpu_write<pic::Impl, uint8_t, &pic::Impl::write8elcr<true>> : cpu_write<pic::Impl, uint8_t, &pic::Impl::write8elcr<false>>
			},
			this, is_update, is_update))) {
			throw std::runtime_error(lv2str(highest, "Failed to update elcr io ports"));
		}
	}
}

void
pic::Impl::reset()
{
	vector_offset = 0;
	imr = 0xFF;
	irr = 0;
	isr = 0;
	elcr = 0;
	in_init = 0;
	read_isr = 0;
	pin_state = 0;
}

void pic::Impl::init(machine *machine, unsigned idx)
{
	m_lc86cpu = machine->get86cpu();
	m_picImpl[idx] = this;
	m_idx = idx;
	if (idx == 0) {
		cpu_set_int_func(m_lc86cpu, { pic::Impl::getInterruptForCpu, this });
	}

	updateIo(false);
	reset();
}

/** Public interface implementation **/
void pic::init(machine *machine, unsigned idx)
{
	m_impl->init(machine, idx);
}

void pic::reset()
{
	m_impl->reset();
}

void pic::updateIoLogging()
{
	m_impl->updateIoLogging();
}

void pic::raiseIrq(uint8_t a)
{
	std::unique_lock lock(pic::Impl::m_mtx);
	m_impl->raiseIrq(a);
}

void pic::lowerIrq(uint8_t a)
{
	std::unique_lock lock(pic::Impl::m_mtx);
	m_impl->lowerIrq(a);
}

pic::pic() : m_impl{std::make_unique<pic::Impl>()} {}
pic::~pic() {}
