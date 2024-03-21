// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pfb

#define NV_PFB 0x00100000
#define NV_PFB_BASE (NV2A_REGISTER_BASE + NV_PFB)
#define NV_PFB_SIZE 0x1000

#define NV_PFB_CFG0 (NV2A_REGISTER_BASE + 0x00100200)
#define NV_PFB_CFG1 (NV2A_REGISTER_BASE + 0x00100204)
#define NV_PFB_CSTATUS (NV2A_REGISTER_BASE + 0x0010020C)
#define NV_PFB_NVM (NV2A_REGISTER_BASE + 0x00100214)


template<bool log, bool enabled>
void pfb::write(uint32_t addr, const uint32_t data)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		log_io_write();
	}

	switch (addr)
	{
	case NV_PFB_CFG0:
		cfg0 = data;
		break;

	case NV_PFB_CFG1:
		cfg1 = data;
		break;

	case NV_PFB_CSTATUS:
		// This register is read-only
		break;

	case NV_PFB_NVM:
		nvm = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log, bool enabled>
uint32_t pfb::read(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value = 0;

	switch (addr)
	{
	case NV_PFB_CFG0:
		value = cfg0;
		break;

	case NV_PFB_CFG1:
		value = cfg1;
		break;

	case NV_PFB_CSTATUS:
		value = cstatus;
		break;

	case NV_PFB_NVM:
		value = nvm;
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
auto pfb::get_io_func(bool log, bool enabled)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return cpu_write<pfb, uint32_t, &pfb::write<true>>;
			}
			else {
				return cpu_write<pfb, uint32_t, &pfb::write<false>>;
			}
		}
		else {
			return cpu_write<pfb, uint32_t, &pfb::write<false, false>>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return cpu_read<pfb, uint32_t, &pfb::read<true>>;
			}
			else {
				return cpu_read<pfb, uint32_t, &pfb::read<false>>;
			}
		}
		else {
			return cpu_read<pfb, uint32_t, &pfb::read<false, false>>;
		}
	}
}

bool
pfb::update_io(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_machine->get<pmc>().engine_enabled & NV_PMC_ENABLE_PFB;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFB_BASE, NV_PFB_SIZE, false,
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
pfb::reset()
{
	// Values dumped from a Retail 1.0 xbox
	cfg0 = 0x03070003;
	cfg1 = 0x11448000;
	nvm = 0; // unknown initial value
	cstatus = m_machine->get<cpu>().get_ramsize();
}

bool
pfb::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
