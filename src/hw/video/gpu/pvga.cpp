// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pvga

#define NV_PRMVGA 0x000A0000
#define NV_PRMVGA_BASE (NV2A_REGISTER_BASE + NV_PRMVGA)
#define NV_PRMVGA_SIZE 0x20000
#define NV_PRMVIO 0x000C0000
#define NV_PRMVIO_BASE (NV2A_REGISTER_BASE + NV_PRMVIO)
#define NV_PRMVIO_SIZE 0x8000
#define NV_PRMCIO 0x00601000
#define NV_PRMCIO_BASE (NV2A_REGISTER_BASE + NV_PRMCIO)
#define NV_PRMCIO_SIZE 0x1000
#define NV_PRMDIO 0x00681000
#define NV_PRMDIO_BASE (NV2A_REGISTER_BASE + NV_PRMDIO)
#define NV_PRMDIO_SIZE 0x1000


template<bool log>
uint8_t pvga::io_read8(uint32_t addr)
{
	uint8_t value = m_machine->get<vga>().io_read8(addr);

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void pvga::io_write8(uint32_t addr, const uint8_t data)
{
	uint8_t value = data;
	if constexpr (log) {
		log_io_write();
	}

	m_machine->get<vga>().io_write8(addr, value);
}

template<bool log>
void pvga::io_write16(uint32_t addr, const uint16_t data)
{
	uint16_t value = data;
	if constexpr (log) {
		log_io_write();
	}

	m_machine->get<vga>().io_write16(addr, value);
}

template<bool log>
uint8_t pvga::mem_read8(uint32_t addr)
{
	uint8_t value = m_machine->get<vga>().mem_read8(addr);

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
uint16_t pvga::mem_read16(uint32_t addr)
{
	uint16_t value = m_machine->get<vga>().mem_read16(addr);

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void pvga::mem_write8(uint32_t addr, const uint8_t data)
{
	uint8_t value = data;
	if constexpr (log) {
		log_io_write();
	}

	m_machine->get<vga>().mem_write8(addr, value);
}

template<bool log>
void pvga::mem_write16(uint32_t addr, const uint16_t data)
{
	uint16_t value = data;
	if constexpr (log) {
		log_io_write();
	}

	m_machine->get<vga>().mem_write16(addr, value);
}

bool
pvga::update_io(bool is_update)
{
	bool log = module_enabled();
	// PRMVIO is an alias for the vga sequencer and graphics controller ports
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMVIO_BASE, NV_PRMVIO_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga, uint8_t, &pvga::io_read8<true>, NV_PRMVIO_BASE> : cpu_read<pvga, uint8_t, &pvga::io_read8<false>, NV_PRMVIO_BASE>,
			.fnw8 = log ? cpu_write<pvga, uint8_t, &pvga::io_write8<true>, NV_PRMVIO_BASE> : cpu_write<pvga, uint8_t, &pvga::io_write8<false>, NV_PRMVIO_BASE>,
			.fnw16 = log ? cpu_write<pvga, uint16_t, &pvga::io_write16<true>, NV_PRMVIO_BASE> : cpu_write<pvga, uint16_t, &pvga::io_write16<false>, NV_PRMVIO_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	// PRMCIO is an alias for the vga attribute controller and crt controller ports
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMCIO_BASE, NV_PRMCIO_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga, uint8_t, &pvga::io_read8<true>, NV_PRMCIO_BASE> : cpu_read<pvga, uint8_t, &pvga::io_read8<false>, NV_PRMCIO_BASE>,
			.fnw8 = log ? cpu_write<pvga, uint8_t, &pvga::io_write8<true>, NV_PRMCIO_BASE> : cpu_write<pvga, uint8_t, &pvga::io_write8<false>, NV_PRMCIO_BASE>,
			.fnw16 = log ? cpu_write<pvga, uint16_t, &pvga::io_write16<true>, NV_PRMCIO_BASE> : cpu_write<pvga, uint16_t, &pvga::io_write16<false>, NV_PRMCIO_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	// PRMDIO is an alias for the vga digital-to-analog converter (DAC) ports
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMDIO_BASE, NV_PRMDIO_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga, uint8_t, &pvga::io_read8<true>, NV_PRMDIO_BASE> : cpu_read<pvga, uint8_t, &pvga::io_read8<false>, NV_PRMDIO_BASE>,
			.fnw8 = log ? cpu_write<pvga, uint8_t, &pvga::io_write8<true>, NV_PRMDIO_BASE> : cpu_write<pvga, uint8_t, &pvga::io_write8<false>, NV_PRMDIO_BASE>,
			.fnw16 = log ? cpu_write<pvga, uint16_t, &pvga::io_write16<true>, NV_PRMDIO_BASE> : cpu_write<pvga, uint16_t, &pvga::io_write16<false>, NV_PRMDIO_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	// PRMVGA is an alias for the vga memory window
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMVGA_BASE, NV_PRMVGA_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga, uint8_t, &pvga::mem_read8<true>, NV_PRMVGA_BASE> : cpu_read<pvga, uint8_t, &pvga::mem_read8<false>, NV_PRMVGA_BASE>,
			.fnr16 = log ? cpu_read<pvga, uint16_t, &pvga::mem_read16<true>, NV_PRMVGA_BASE> : cpu_read<pvga, uint16_t, &pvga::mem_read16<false>, NV_PRMVGA_BASE>,
			.fnw8 = log ? cpu_write<pvga, uint8_t, &pvga::mem_write8<true>, NV_PRMVGA_BASE> : cpu_write<pvga, uint8_t, &pvga::mem_write8<false>, NV_PRMVGA_BASE>,
			.fnw16 = log ? cpu_write<pvga, uint16_t, &pvga::mem_write16<true>, NV_PRMVGA_BASE> : cpu_write<pvga, uint16_t, &pvga::mem_write16<false>, NV_PRMVGA_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pvga::reset()
{
	m_machine->get<vga>().reset();
}

bool
pvga::init()
{
	if (!update_io(false)) {
		return false;
	}

	// Don't reset here, because vga will be reset when it's initalized later
	return true;
}
