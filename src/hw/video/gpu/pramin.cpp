// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pramin

#define NV_PRAMIN 0x00700000
#define NV_PRAMIN_BASE (NV2A_REGISTER_BASE + NV_PRAMIN)
#define NV_PRAMIN_SIZE 0x100000 // = 1 MiB

#define RAMIN_UNIT_SIZE 64


template<bool log>
uint8_t pramin::read8(uint32_t addr)
{
	uint8_t value = m_ram[ramin_to_ram_addr(addr)];

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
uint16_t pramin::read16(uint32_t addr)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	uint16_t value = *(uint16_t *)ram_ptr;

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
uint32_t pramin::read32(uint32_t addr)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	uint32_t value = *(uint32_t *)ram_ptr;

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void pramin::write8(uint32_t addr, const uint8_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	m_ram[ramin_to_ram_addr(addr)] = data;
}

template<bool log>
void pramin::write16(uint32_t addr, const uint16_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	*(uint16_t *)ram_ptr = data;
}

template<bool log>
void pramin::write32(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	*(uint32_t *)ram_ptr = data;
}

uint32_t
pramin::ramin_to_ram_addr(uint32_t ramin_addr)
{
	ramin_addr -= NV_PRAMIN_BASE;
	return m_machine->get<pfb>().cstatus - (ramin_addr - (ramin_addr % RAMIN_UNIT_SIZE)) - RAMIN_UNIT_SIZE + (ramin_addr % RAMIN_UNIT_SIZE);
}

bool
pramin::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRAMIN_BASE, NV_PRAMIN_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pramin, uint8_t, &pramin::read8<true>> : cpu_read<pramin, uint8_t, &pramin::read8<false>>,
			.fnr16 = log ? cpu_read<pramin, uint16_t, &pramin::read16<true>> : cpu_read<pramin, uint16_t, &pramin::read16<false>>,
			.fnr32 = log ? cpu_read<pramin, uint32_t, &pramin::read32<true>> : cpu_read<pramin, uint32_t, &pramin::read32<false>>,
			.fnw8 = log ? cpu_write<pramin, uint8_t, &pramin::write8<true>> : cpu_write<pramin, uint8_t, &pramin::write8<false>>,
			.fnw16 = log ? cpu_write<pramin, uint16_t, &pramin::write16<true>> : cpu_write<pramin, uint16_t, &pramin::write16<false>>,
			.fnw32 = log ? cpu_write<pramin, uint32_t, &pramin::write32<true>> : cpu_write<pramin, uint32_t, &pramin::write32<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

bool
pramin::init()
{
	// Tested and confirmed with a Retail 1.0 xbox. The ramin starts from the end of vram, and it's the last MiB of it. It's also addressed in reverse order,
	// with block units of 64 bytes each.
	/*
	ramin -> vram
	0 -> 0xF3FFFFC0
	32 -> 0xF3FFFFE0
	64 -> 0xF3FFFF80
	96 -> 0xF3FFFFA0
	
	- 32 bytes, -- 64 bytes block units
	----------ramin
	abcd  efgh
	ghef  cdab
	----------vram
	*/

	if (!update_io(false)) {
		return false;
	}

	m_ram = get_ram_ptr(m_machine->get<cpu_t *>());
	return true;
}
