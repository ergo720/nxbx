// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pramin

#define RAMIN_UNIT_SIZE 64


template<typename T, bool log>
T pramin::read(uint32_t addr)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	T value = *(T *)ram_ptr;

	if constexpr (log) {
		log_read(addr, value);
	}

	return value;
}

template<typename T, bool log>
void pramin::write(uint32_t addr, const T value)
{
	if constexpr (log) {
		log_write(addr, value);
	}

	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr);
	*(T *)ram_ptr = value;
}

uint32_t
pramin::ramin_to_ram_addr(uint32_t ramin_addr)
{
	ramin_addr -= NV_PRAMIN_BASE;
	return m_machine->get<pfb>().m_regs[REGS_PFB_idx(NV_PFB_CSTATUS)] - (ramin_addr - (ramin_addr % RAMIN_UNIT_SIZE)) - RAMIN_UNIT_SIZE + (ramin_addr % RAMIN_UNIT_SIZE);
}

void
pramin::log_read(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pramin, false>("Read at NV_PRAMIN_BASE + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PRAMIN_BASE, addr, value);
}

void
pramin::log_write(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pramin, false>("Write at NV_PRAMIN_BASE + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PRAMIN_BASE, addr, value);
}

template<bool is_write, typename T>
auto pramin::get_io_func(bool log, bool is_be)
{
	if constexpr (is_write) {
		if (log) {
			return is_be ? nv2a_write<pramin, T, &pramin::write<T, true>, big> : nv2a_write<pramin, T, &pramin::write<T, true>, le>;
		}
		else {
			return is_be ? nv2a_write<pramin, T, &pramin::write<T, false>, big> : nv2a_write<pramin, T, &pramin::write<T, false>, le>;
		}
	}
	else {
		if (log) {
			return is_be ? nv2a_read<pramin, T, &pramin::read<T, true>, big> : nv2a_read<pramin, T, &pramin::read<T, true>, le>;
		}
		else {
			return is_be ? nv2a_read<pramin, T, &pramin::read<T, false>, big> : nv2a_read<pramin, T, &pramin::read<T, false>, le>;
		}
	}
}

bool
pramin::update_io(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRAMIN_BASE, NV_PRAMIN_SIZE, false,
		{
			.fnr8 = get_io_func<false, uint8_t>(log, is_be),
			.fnr16 = get_io_func<false, uint16_t>(log, is_be),
			.fnr32 = get_io_func<false, uint32_t>(log, is_be),
			.fnw8 = get_io_func<true, uint8_t>(log, is_be),
			.fnw16 = get_io_func<true, uint16_t>(log, is_be),
			.fnw32 = get_io_func<true, uint32_t>(log, is_be),
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
