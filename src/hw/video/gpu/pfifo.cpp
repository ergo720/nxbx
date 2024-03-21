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


template<bool log, bool enabled>
void pfifo::write(uint32_t addr, const uint32_t data)
{
	if constexpr (!enabled) {
		return;
	}
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

template<bool log, bool enabled>
uint32_t pfifo::read(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

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

template<bool is_write>
auto pfifo::get_io_func(bool log, bool enabled)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return cpu_write<pfifo, uint32_t, &pfifo::write<true>>;
			}
			else {
				return cpu_write<pfifo, uint32_t, &pfifo::write<false>>;
			}
		}
		else {
			return cpu_write<pfifo, uint32_t, &pfifo::write<false, false>>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return cpu_read<pfifo, uint32_t, &pfifo::read<true>>;
			}
			else {
				return cpu_read<pfifo, uint32_t, &pfifo::read<false>>;
			}
		}
		else {
			return cpu_read<pfifo, uint32_t, &pfifo::read<false, false>>;
		}
	}
}

bool
pfifo::update_io(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_machine->get<pmc>().engine_enabled & NV_PMC_ENABLE_PFIFO;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFIFO_BASE, NV_PFIFO_SIZE, false,
		{
			.fnr32 = get_io_func<false>(log, enabled),
			.fnw32 = get_io_func<true>(log, enabled)
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
