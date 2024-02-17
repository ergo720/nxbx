// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../cpu.hpp"
#include "nv2a.hpp"

#define NV_PCRTC 0x00600000
#define NV_PCRTC_BASE (NV2A_REGISTER_BASE + NV_PCRTC)
#define NV_PCRTC_SIZE 0x1000

#define NV_PCRTC_INTR_0 (NV2A_REGISTER_BASE + 0x00600100)
#define NV_PCRTC_INTR_0_VBLANK_NOT_PENDING 0x00000000
#define NV_PCRTC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00600140)
#define NV_PCRTC_INTR_EN_0_VBLANK_DISABLED 0x00000000


static void
pcrtc_write(uint32_t addr, const uint32_t data, void *opaque)
{
	switch (addr)
	{
	case NV_PCRTC_INTR_0:
		g_nv2a.pcrtc.int_status &= ~data;
		pmc_update_irq();
		break;

	case NV_PCRTC_INTR_EN_0:
		g_nv2a.pcrtc.int_enabled = data;
		pmc_update_irq();
		break;

	default:
		nxbx_fatal("Unhandled PCRTC write at address 0x%X with value 0x%X", addr, data);
	}
}

static uint32_t
pcrtc_read(uint32_t addr, void *opaque)
{
	uint32_t value = std::numeric_limits<uint32_t>::max();

	switch (addr)
	{
	case NV_PCRTC_INTR_0:
		value = g_nv2a.pcrtc.int_status;
		break;

	case NV_PCRTC_INTR_EN_0:
		value = g_nv2a.pcrtc.int_enabled;
		break;

	default:
		nxbx_fatal("Unhandled PCRTC read at address 0x%X", addr);
	}

	return value;
}

static void
pcrtc_reset()
{
	g_nv2a.pcrtc.int_status = NV_PCRTC_INTR_0_VBLANK_NOT_PENDING;
	g_nv2a.pcrtc.int_enabled = NV_PCRTC_INTR_EN_0_VBLANK_DISABLED;
}

void
pcrtc_init()
{
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, NV_PCRTC_BASE, NV_PCRTC_SIZE, false, { .fnr32 = pcrtc_read, .fnw32 = pcrtc_write }, nullptr))) {
		throw nxbx_exp_abort("Failed to initialize pcrtc MMIO range");
	}

	pcrtc_reset();
}
