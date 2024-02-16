// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../cpu.hpp"
#include "nv2a.hpp"

#define NV_PMC 0x00000000
#define NV_PMC_BASE (NV2A_REGISTER_BASE + NV_PMC)
#define NV_PMC_SIZE 0x1000

#define NV_PMC_BOOT_0 0x00000000
#define NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0 0x02A000A3
#define NV_PMC_BOOT_1 0x00000004
#define NV_PMC_BOOT_1_ENDIAN00_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN00_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN24_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK (0x00000000 << 0)
#define NV_PMC_BOOT_1_ENDIAN0_BIG_MASK (0x00000001 << 0)
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE_MASK (0x00000000 << 24)
#define NV_PMC_BOOT_1_ENDIAN24_BIG_MASK (0x00000001 << 24)


struct pmc_t {
	uint32_t endianness;
} g_pmc;

static void
pmc_write(uint32_t addr, const uint32_t data, void *opaque)
{
	addr -= NV2A_REGISTER_BASE;

	switch (addr)
	{
	case NV_PMC_BOOT_0:
		// This register is read-only
		break;

	case NV_PMC_BOOT_1: {
		// This register switches the endianness of all accesses done through BAR0 and BAR2/3 (when present)
		uint32_t mask = NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK;
		if (data & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK) {
			mask = NV_PMC_BOOT_1_ENDIAN0_BIG_MASK;
			nxbx_fatal("NV_PMC_BOOT_1: big endian switch not implemented");
			break;
		}
		g_pmc.endianness = ((data & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK) | mask);
	}
	break;

	default:
		nxbx_fatal("Unhandled PMC write at address 0x%X with value 0x%X", addr, data);
	}
}

static uint32_t
pmc_read(uint32_t addr, void *opaque)
{
	addr -= NV2A_REGISTER_BASE;
	uint32_t value = std::numeric_limits<uint32_t>::max();

	switch (addr)
	{
	case NV_PMC_BOOT_0:
		// Returns the id of the gpu
		value = NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0; // value dumped from a Retail 1.0 xbox
		break;

	case NV_PMC_BOOT_1:
		// Returns the current endianness used for MMIO accesses to the gpu
		value = g_pmc.endianness;
		break;

	default:
		nxbx_fatal("Unhandled PMC read at address 0x%X", addr);
	}

	return value;
}

static void
pmc_reset()
{
	g_pmc.endianness = NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK | NV_PMC_BOOT_1_ENDIAN24_LITTLE_MASK;
}

void
pmc_init()
{
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, NV_PMC_BASE, NV_PMC_SIZE, false, { .fnr32 = pmc_read, .fnw32 = pmc_write }, nullptr))) {
		throw nxbx_exp_abort("Failed to initialize pmc MMIO range");
	}

	pmc_reset();
}
