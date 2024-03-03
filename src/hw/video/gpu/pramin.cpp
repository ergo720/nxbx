// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define NV_PRAMIN 0x00700000
#define NV_PRAMIN_BASE (NV2A_REGISTER_BASE + NV_PRAMIN)
#define NV_PRAMIN_SIZE 0x100000 // = 1 MiB

#define RAMIN_UNIT_SIZE 64


uint8_t
pramin::read8(uint32_t addr)
{
	return m_ram[ramin_to_ram_addr(addr)];
}

uint16_t
pramin::read16(uint32_t addr)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	return *(uint16_t *)ram_ptr;
}

uint32_t
pramin::read32(uint32_t addr)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	return *(uint32_t *)ram_ptr;
}

void
pramin::write8(uint32_t addr, const uint8_t data)
{
	m_ram[ramin_to_ram_addr(addr)] = data;
}

void
pramin::write16(uint32_t addr, const uint16_t data)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	*(uint16_t *)ram_ptr = data;
}

void
pramin::write32(uint32_t addr, const uint32_t data)
{
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

	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRAMIN_BASE, NV_PRAMIN_SIZE, false,
		{
			.fnr8 = cpu_read<pramin, uint8_t, &pramin::read8>,
			.fnr16 = cpu_read<pramin, uint16_t, &pramin::read16>,
			.fnr32 = cpu_read<pramin, uint32_t, &pramin::read32>,
			.fnw8 = cpu_write<pramin, uint8_t, &pramin::write8>,
			.fnw16 = cpu_write<pramin, uint16_t, &pramin::write16>,
			.fnw32 = cpu_write<pramin, uint32_t, &pramin::write32>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio region", get_name());
		return false;
	}

	m_ram = get_ram_ptr(m_machine->get<cpu_t *>());
	return true;
}
