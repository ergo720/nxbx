// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define NV_PFB 0x00100000
#define NV_PFB_BASE (NV2A_REGISTER_BASE + NV_PFB)
#define NV_PFB_SIZE 0x1000

#define NV_PFB_CFG0 (NV2A_REGISTER_BASE + 0x00100200)
#define NV_PFB_CFG1 (NV2A_REGISTER_BASE + 0x00100204)
#define NV_PFB_CSTATUS (NV2A_REGISTER_BASE + 0x0010020C)
#define NV_PFB_NVM (NV2A_REGISTER_BASE + 0x00100214)


void
pfb::write(uint32_t addr, const uint32_t data)
{
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
		logger(log_lv::warn, "NV_PFB_NVM: functionality unknown");
		nvm = data;
		break;

	default:
		nxbx::fatal("Unhandled %s write at address 0x%" PRIX32 " with value 0x%" PRIX32, get_name(), addr, data);
	}
}

uint32_t
pfb::read(uint32_t addr)
{
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
		nxbx::fatal("Unhandled %s read at address 0x%" PRIX32, get_name(), addr);
	}

	return value;
}

void
pfb::reset()
{
	// Values dumped from a Retail 1.0 xbox
	cfg0 = 0x03070003;
	cfg1 = 0x11448000;
	nvm = 0; // unknown initial value
	cstatus = NV2A_FB_SIZE;
}

bool
pfb::init()
{
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFB_BASE, NV_PFB_SIZE, false,
		{
			.fnr32 = cpu_read<pfb, uint32_t, &pfb::read>,
			.fnw32 = cpu_write<pfb, uint32_t, &pfb::write>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio ports", get_name());
		return false;
	}

	reset();
	return true;
}
