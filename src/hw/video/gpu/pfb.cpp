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
		nvm = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
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
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	return value;
}

uint32_t
pfb::read_logger(uint32_t addr)
{
	uint32_t data = read(addr);
	log_io_read();
	return data;
}

void
pfb::write_logger(uint32_t addr, const uint32_t data)
{
	log_io_write();
	write(addr, data);
}

bool
pfb::update_io(bool is_update)
{
	bool enable = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFB_BASE, NV_PFB_SIZE, false,
		{
			.fnr32 = enable ? cpu_read<pfb, uint32_t, &pfb::read_logger> : cpu_read<pfb, uint32_t, &pfb::read>,
			.fnw32 = enable ? cpu_write<pfb, uint32_t, &pfb::write_logger> : cpu_write<pfb, uint32_t, &pfb::write>
		},
		this, is_update, is_update))) {
		loggerex1(error, "Failed to update mmio region");
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
