// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../cpu.hpp"
#include "pic.hpp"
#include "nv2a.hpp"

#define NV_PMC 0x00000000
#define NV_PMC_BASE (NV2A_REGISTER_BASE + NV_PMC)
#define NV_PMC_SIZE 0x1000

#define NV_PMC_BOOT_0 (NV2A_REGISTER_BASE + 0x00000000)
#define NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0 0x02A000A3
#define NV_PMC_BOOT_1 (NV2A_REGISTER_BASE + 0x00000004)
#define NV_PMC_BOOT_1_ENDIAN00_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN00_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN24_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK (0x00000000 << 0)
#define NV_PMC_BOOT_1_ENDIAN0_BIG_MASK (0x00000001 << 0)
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE_MASK (0x00000000 << 24)
#define NV_PMC_BOOT_1_ENDIAN24_BIG_MASK (0x00000001 << 24)
#define NV_PMC_INTR_0 (NV2A_REGISTER_BASE + 0x00000100)
#define NV_PMC_INTR_0_PTIMER 20
#define NV_PMC_INTR_0_PCRTC 24
#define NV_PMC_INTR_0_SOFTWARE 31
#define NV_PMC_INTR_0_NOT_PENDING 0x00000000
#define NV_PMC_INTR_0_HARDWARE_MASK (~(1 << NV_PMC_INTR_0_SOFTWARE))
#define NV_PMC_INTR_0_SOFTWARE_MASK (1 << NV_PMC_INTR_0_SOFTWARE)
#define NV_PMC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00000140)
#define NV_PMC_INTR_EN_0_INTA_DISABLED 0x00000000
#define NV_PMC_INTR_EN_0_INTA_HARDWARE 0x00000001
#define NV_PMC_INTR_EN_0_INTA_SOFTWARE 0x00000002


static void
pmc_write(uint32_t addr, const uint32_t data, void *opaque)
{
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
		g_nv2a.pmc.endianness = ((data & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK) | mask);
	}
	break;

	case NV_PMC_INTR_0:
		// Only NV_PMC_INTR_0_SOFTWARE is writable, the other bits are read-only
		g_nv2a.pmc.int_status = (g_nv2a.pmc.int_status & ~NV_PMC_INTR_0_SOFTWARE_MASK) | (data & NV_PMC_INTR_0_SOFTWARE_MASK);
		pmc_update_irq();
		break;

	case NV_PMC_INTR_EN_0:
		g_nv2a.pmc.int_enabled = data;
		pmc_update_irq();
		break;

	default:
		nxbx_fatal("Unhandled PMC write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

static uint32_t
pmc_read(uint32_t addr, void *opaque)
{
	uint32_t value = std::numeric_limits<uint32_t>::max();

	switch (addr)
	{
	case NV_PMC_BOOT_0:
		// Returns the id of the gpu
		value = NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0; // value dumped from a Retail 1.0 xbox
		break;

	case NV_PMC_BOOT_1:
		// Returns the current endianness used for MMIO accesses to the gpu
		value = g_nv2a.pmc.endianness;
		break;

	case NV_PMC_INTR_0:
		value = g_nv2a.pmc.int_status;
		break;

	case NV_PMC_INTR_EN_0:
		value = g_nv2a.pmc.int_enabled;
		break;

	default:
		nxbx_fatal("Unhandled PMC read at address 0x%" PRIX32, addr);
	}

	return value;
}

void
pmc_update_irq()
{
	// Check for pending PCRTC interrupts
	if (g_nv2a.pcrtc.int_status & g_nv2a.pcrtc.int_enabled) {
		g_nv2a.pmc.int_status |= (1 << NV_PMC_INTR_0_PCRTC);
	}
	else {
		g_nv2a.pmc.int_status &= ~(1 << NV_PMC_INTR_0_PCRTC);
	}

	// Check for pending PTIMER interrupts
	if (g_nv2a.ptimer.int_status & g_nv2a.ptimer.int_enabled) {
		g_nv2a.pmc.int_status |= (1 << NV_PMC_INTR_0_PTIMER);
	}
	else {
		g_nv2a.pmc.int_status &= ~(1 << NV_PMC_INTR_0_PTIMER);
	}

	switch (g_nv2a.pmc.int_enabled)
	{
	default:
	case NV_PMC_INTR_EN_0_INTA_DISABLED:
		// Don't do anything
		break;

	case NV_PMC_INTR_EN_0_INTA_HARDWARE:
		if (g_nv2a.pmc.int_status & NV_PMC_INTR_0_HARDWARE_MASK) {
			pic_raise_irq(NV2A_IRQ_NUM);
		}
		else {
			pic_lower_irq(NV2A_IRQ_NUM);
		}
		break;

	case NV_PMC_INTR_EN_0_INTA_SOFTWARE:
		if (g_nv2a.pmc.int_status & NV_PMC_INTR_0_SOFTWARE_MASK) {
			pic_raise_irq(NV2A_IRQ_NUM);
		}
		else {
			pic_lower_irq(NV2A_IRQ_NUM);
		}
		break;
	}
}

static void
pmc_reset()
{
	g_nv2a.pmc.endianness = NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK | NV_PMC_BOOT_1_ENDIAN24_LITTLE_MASK;
	g_nv2a.pmc.int_status = NV_PMC_INTR_0_NOT_PENDING;
	g_nv2a.pmc.int_enabled = NV_PMC_INTR_EN_0_INTA_DISABLED;
}

void
pmc_init()
{
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, NV_PMC_BASE, NV_PMC_SIZE, false, { .fnr32 = pmc_read, .fnw32 = pmc_write }, nullptr))) {
		throw nxbx_exp_abort("Failed to initialize pmc MMIO range");
	}

	pmc_reset();
}
