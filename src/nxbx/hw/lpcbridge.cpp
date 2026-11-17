// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720


#include "lib86cpu.hpp"
#include "machine.hpp"
#include "host.hpp"
#include "lpcbridge.hpp"
#include "cpu.hpp"
#include <cinttypes>

#define MODULE_NAME lpcbridge

#define SCI_IRQ_NUM 12

#define ACPI_GPE0_STATUS           0x8020
#define ACPI_GPIO_FIELD            0x80C0
#define ACPI_GPE0_EXTSMI_STATUS    0x02


/** Private device implementation **/
class lpcbridge::Impl
{
public:
	void init(machine *machine);
	void reset();
	void updateIoLogging() { updateIo(true); }
	void triggerInterrupt() { m_machine->raise_irq(SCI_IRQ_NUM); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);

private:
	void updateIo(bool is_update);

	uint8_t m_field_pin;
	// connected devices
	machine *m_machine;
	cpu_t *m_lc86cpu;
	// registers
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ ACPI_GPE0_STATUS, "ACPI_GPE0_STATUS" },
		{ ACPI_GPIO_FIELD, "ACPI_GPIO_FIELD" },
	};
};

template<bool log>
uint8_t lpcbridge::Impl::read8(uint32_t addr)
{
	uint8_t value;

	switch (addr)
	{
	case ACPI_GPIO_FIELD: // bit 5 indicates the current TV display field (even/odd), other bits are unknown
		value = (++m_field_pin & 1) << 5;
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
		value = 0;
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void lpcbridge::Impl::write8(uint32_t addr, const uint8_t value)
{
	if constexpr (log) {
		log_io_write();
	}

	switch (addr)
	{
	case ACPI_GPE0_STATUS:
		if (value & ACPI_GPE0_EXTSMI_STATUS) {
			m_machine->lower_irq(SCI_IRQ_NUM);
		}
		break;

	case ACPI_GPIO_FIELD:
		// d3d writes 1 to this at startup, enables field reporting?
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

void lpcbridge::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, 0x8000, 0x100, true,
		{
			.fnr8 = log ? cpu_read<lpcbridge::Impl, uint8_t, &lpcbridge::Impl::read8<true>> : cpu_read<lpcbridge::Impl, uint8_t, &lpcbridge::Impl::read8<false>>,
			.fnw8 = log ? cpu_write<lpcbridge::Impl, uint8_t, &lpcbridge::Impl::write8<true>> : cpu_write<lpcbridge::Impl, uint8_t, &lpcbridge::Impl::write8<false>>,
		},
		this, is_update, is_update))) {
		throw std::runtime_error("Failed to update lpc bridge io ports");
	}
}

void lpcbridge::Impl::reset()
{
	m_field_pin = 0;
}

void lpcbridge::Impl::init(machine *machine)
{
	m_lc86cpu = machine->get86cpu();
	m_machine = machine;
	updateIo(false);
	reset();
}

/** Public interface implementation **/
void lpcbridge::init(machine *machine)
{
	m_impl->init(machine);
}

void lpcbridge::reset()
{
	m_impl->reset();
}

void lpcbridge::updateIoLogging()
{
	m_impl->updateIoLogging();
}

void lpcbridge::triggerInterrupt()
{
	m_impl->triggerInterrupt();
}

lpcbridge::lpcbridge() : m_impl{std::make_unique<lpcbridge::Impl>()} {}
lpcbridge::~lpcbridge() {}
