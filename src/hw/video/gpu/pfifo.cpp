// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pfifo

#define NV_PFIFO 0x00002000
#define NV_PFIFO_BASE (NV2A_REGISTER_BASE + NV_PFIFO)
#define NV_PFIFO_SIZE 0x2000

#define NV_PFIFO_RAMHT (NV2A_REGISTER_BASE + 0x00002210)
#define NV_PFIFO_RAMFC (NV2A_REGISTER_BASE + 0x00002214)
#define NV_PFIFO_RAMRO (NV2A_REGISTER_BASE + 0x00002218)


template<bool log>
void pfifo::write(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	switch (addr)
	{
	case NV_PFIFO_RAMHT:
		ramht = data;
		break;

	case NV_PFIFO_RAMFC:
		ramfc = data;
		break;

	case NV_PFIFO_RAMRO:
		ramro = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log>
uint32_t pfifo::read(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PFIFO_RAMHT:
		value = ramht;
		break;

	case NV_PFIFO_RAMFC:
		value = ramfc;
		break;

	case NV_PFIFO_RAMRO:
		value = ramro;
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

bool
pfifo::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFIFO_BASE, NV_PFIFO_SIZE, false,
		{
			.fnr32 = log ? cpu_read<pfifo, uint32_t, &pfifo::read<true>> : cpu_read<pfifo, uint32_t, &pfifo::read<false>>,
			.fnw32 = log ? cpu_write<pfifo, uint32_t, &pfifo::write<true>> : cpu_write<pfifo, uint32_t, &pfifo::write<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pfifo::reset()
{
	// Values dumped from a Retail 1.0 xbox
	ramht = 0x00000100;
	ramfc = 0x008A0110;
	ramro = 0x00000114;
}

bool
pfifo::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
