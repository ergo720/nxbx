// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../../clock.hpp"
#include "../../cpu.hpp"
#include "nv2a.hpp"

#define NV_PRAMDAC 0x00680300
#define NV_PRAMDAC_BASE (NV2A_REGISTER_BASE + NV_PRAMDAC)
#define NV_PRAMDAC_SIZE 0xD00

#define NV_PRAMDAC_NVPLL_COEFF (NV2A_REGISTER_BASE + 0x00680500)
#define NV_PRAMDAC_NVPLL_COEFF_MDIV_MASK 0x000000FF
#define NV_PRAMDAC_NVPLL_COEFF_NDIV_MASK 0x0000FF00
#define NV_PRAMDAC_NVPLL_COEFF_PDIV_MASK 0x00070000
#define NV_PRAMDAC_MPLL_COEFF (NV2A_REGISTER_BASE + 0x00680504)
#define NV_PRAMDAC_VPLL_COEFF (NV2A_REGISTER_BASE + 0x00680508)


static void
pramdac_write32(uint32_t addr, const uint32_t data, void *opaque)
{
	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF: {
		// NOTE: if the m value is zero, then the final frequency is also zero
		g_nv2a.pramdac.nvpll_coeff = data;
		uint64_t m = data & NV_PRAMDAC_NVPLL_COEFF_MDIV_MASK;
		uint64_t n = (data & NV_PRAMDAC_NVPLL_COEFF_NDIV_MASK) >> 8;
		uint64_t p = (data & NV_PRAMDAC_NVPLL_COEFF_PDIV_MASK) >> 16;
		g_nv2a.pramdac.core_freq = m ? ((NV2A_CRYSTAL_FREQ * n) / (1ULL << p) / m) : 0;
		if (g_nv2a.ptimer.counter_active) {
			g_nv2a.ptimer.counter_period = ptimer_counter_to_us();
			cpu_set_timeout(g_cpu, cpu_check_periodic_events(get_now()));
		}
	}
	break;

	case NV_PRAMDAC_MPLL_COEFF:
		g_nv2a.pramdac.mpll_coeff = data;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		g_nv2a.pramdac.vpll_coeff = data;
		break;

	default:
		nxbx_fatal("Unhandled PRAMDAC write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

static uint32_t
pramdac_read32(uint32_t addr, void *opaque)
{
	uint32_t value = std::numeric_limits<uint32_t>::max();

	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF:
		value = g_nv2a.pramdac.nvpll_coeff;
		break;

	case NV_PRAMDAC_MPLL_COEFF:
		value = g_nv2a.pramdac.mpll_coeff;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		value = g_nv2a.pramdac.vpll_coeff;
		break;

	default:
		nxbx_fatal("Unhandled PRAMDAC read at address 0x%" PRIX32, addr);
	}

	return value;
}

static uint8_t
pramdac_read8(uint32_t addr, void *opaque)
{
	// This handler is necessary because Direct3D_CreateDevice reads the n value by accessing the second byte of the register, even though the coefficient
	// registers are supposed to be four bytes instead. This is probably due to compiler optimizations

	uint32_t value = std::numeric_limits<uint32_t>::max();
	uint32_t addr_base = addr & ~3;
	uint32_t addr_offset = addr & 3;

	switch (addr_base)
	{
	case NV_PRAMDAC_NVPLL_COEFF:
	case NV_PRAMDAC_MPLL_COEFF:
	case NV_PRAMDAC_VPLL_COEFF:
		value = pramdac_read32(addr_base, opaque);
		break;

	default:
		nxbx_fatal("Unhandled PRAMDAC read at address 0x%" PRIX32, addr);
	}

	addr_offset <<= 3;
	return uint8_t((value & (0xFF << addr_offset)) >> addr_offset);
}

static void
pramdac_reset()
{
	// Values dumped from a Retail 1.0 xbox
	g_nv2a.pramdac.core_freq = NV2A_CLOCK_FREQ;
	g_nv2a.pramdac.nvpll_coeff = 0x00011C01;
	g_nv2a.pramdac.mpll_coeff = 0x00007702;
	g_nv2a.pramdac.vpll_coeff = 0x0003C20D;
}

void
pramdac_init()
{
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, NV_PRAMDAC_BASE, NV_PRAMDAC_SIZE, false,
		{ 
		.fnr8 = pramdac_read8,
		.fnr32 = pramdac_read32,
		.fnw32 = pramdac_write32
		},
		nullptr))) {
		throw nxbx_exp_abort("Failed to initialize pramdac MMIO range");
	}

	pramdac_reset();
}
