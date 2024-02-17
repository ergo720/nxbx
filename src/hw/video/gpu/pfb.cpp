// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../cpu.hpp"
#include "nv2a.hpp"

#define NV_PFB 0x00100000
#define NV_PFB_BASE (NV2A_REGISTER_BASE + NV_PFB)
#define NV_PFB_SIZE 0x1000

#define NV_PFB_CSTATUS (NV2A_REGISTER_BASE + 0x0010020C)


static void
pfb_write(uint32_t addr, const uint32_t data, void *opaque)
{
	switch (addr)
	{
	case NV_PFB_CSTATUS:
		// This register is read-only
		break;

	default:
		nxbx_fatal("Unhandled PFB write at address 0x%X with value 0x%X", addr, data);
	}
}

static uint32_t
pfb_read(uint32_t addr, void *opaque)
{
	uint32_t value = std::numeric_limits<uint32_t>::max();

	switch (addr)
	{
	case NV_PFB_CSTATUS:
		// Returns the size of the framebuffer
		value = NV2A_FB_SIZE;
		break;

	default:
		nxbx_fatal("Unhandled PFB read at address 0x%X", addr);
	}

	return value;
}

void
pfb_init()
{
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, NV_PFB_BASE, NV_PFB_SIZE, false, { .fnr32 = pfb_read, .fnw32 = pfb_write }, nullptr))) {
		throw nxbx_exp_abort("Failed to initialize pfb MMIO range");
	}
}
