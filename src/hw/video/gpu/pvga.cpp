// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

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


uint8_t
pvga::io_read8(uint32_t addr)
{
	return m_machine->get<vga>().io_read8(addr);
}

void
pvga::io_write8(uint32_t addr, const uint8_t data)
{
	m_machine->get<vga>().io_write8(addr, data);
}

void
pvga::io_write16(uint32_t addr, const uint16_t data)
{
	m_machine->get<vga>().io_write16(addr, data);
}

uint8_t
pvga::mem_read8(uint32_t addr)
{
	return m_machine->get<vga>().mem_read8(addr);
}

uint16_t
pvga::mem_read16(uint32_t addr)
{
	return m_machine->get<vga>().mem_read16(addr);
}
void
pvga::mem_write8(uint32_t addr, const uint8_t data)
{
	m_machine->get<vga>().mem_write8(addr, data);
}

void
pvga::mem_write16(uint32_t addr, const uint16_t data)
{
	m_machine->get<vga>().mem_write16(addr, data);
}

void
pvga::reset()
{
	m_machine->get<vga>().reset();
}

bool
pvga::init()
{
	// PRMVIO is an alias for the vga sequencer and graphics controller ports
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMVIO_BASE, NV_PRMVIO_SIZE, false,
		{
			.fnr8 = cpu_read<pvga, uint8_t, &pvga::io_read8, NV_PRMVIO_BASE>,
			.fnw8 = cpu_write<pvga, uint8_t, &pvga::io_write8, NV_PRMVIO_BASE>,
			.fnw16 = cpu_write<pvga, uint16_t, &pvga::io_write16, NV_PRMVIO_BASE>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio region", get_name());
		return false;
	}

	// PRMCIO is an alias for the vga attribute controller and crt controller ports
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMCIO_BASE, NV_PRMCIO_SIZE, false,
		{
			.fnr8 = cpu_read<pvga, uint8_t, &pvga::io_read8, NV_PRMCIO_BASE>,
			.fnw8 = cpu_write<pvga, uint8_t, &pvga::io_write8, NV_PRMCIO_BASE>,
			.fnw16 = cpu_write<pvga, uint16_t, &pvga::io_write16, NV_PRMCIO_BASE>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio region", get_name());
		return false;
	}

	// PRMDIO is an alias for the vga digital-to-analog converter (DAC) ports
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMDIO_BASE, NV_PRMDIO_SIZE, false,
		{
			.fnr8 = cpu_read<pvga, uint8_t, &pvga::io_read8, NV_PRMDIO_BASE>,
			.fnw8 = cpu_write<pvga, uint8_t, &pvga::io_write8, NV_PRMDIO_BASE>,
			.fnw16 = cpu_write<pvga, uint16_t, &pvga::io_write16, NV_PRMDIO_BASE>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio region", get_name());
		return false;
	}

	// PRMVGA is an alias for the vga memory window
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRMVGA_BASE, NV_PRMVGA_SIZE, false,
		{
			.fnr8 = cpu_read<pvga, uint8_t, &pvga::mem_read8, NV_PRMVGA_BASE>,
			.fnr16 = cpu_read<pvga, uint16_t, &pvga::mem_read16, NV_PRMVGA_BASE>,
			.fnw8 = cpu_write<pvga, uint8_t, &pvga::mem_write8, NV_PRMVGA_BASE>,
			.fnw16 = cpu_write<pvga, uint16_t, &pvga::mem_write16, NV_PRMVGA_BASE>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio region", get_name());
		return false;
	}

	// Don't reset here, because vga will be reset when it's initalized later
	return true;
}
