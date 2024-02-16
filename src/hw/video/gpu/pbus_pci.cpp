// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../pci.hpp"
#include "../../cpu.hpp"
#include "nv2a.hpp"
#include "pbus_pci.hpp"
#include <assert.h>

#define NV_PBUS_PCI_NV_0 0x00001800
#define NV_PBUS_PCI_BASE (NV2A_REGISTER_BASE + NV_PBUS_PCI_NV_0)


// Values dumped from a Retail 1.0 xbox
static constexpr uint32_t default_pci_configuration[] = {
	0x02A010DE,
	0x02B00007,
	0x030000A1,
	0x0000F800,
	0xFD000000,
	0xF0000008,
	0x00000008,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000060,
	0x00000000,
	0x01050103,
	0x00000000,
	0x00200002,
	0x1F000017,
	0x1F000114,
	0x00000000,
	0x00000001,
	0x0023D6CE,
	0x0000000F,
	0x00024401,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x2B16D065,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

static int
nv2a_pci_write(uint8_t *ptr, uint8_t addr, uint8_t data)
{
	return 0; // pass-through the write
}

static void
pbus_pci_write(uint32_t addr, const uint32_t data, void *opaque)
{
	uint32_t *pci_conf = (uint32_t *)opaque;
	pci_conf[(addr - NV_PBUS_PCI_BASE) / 4] = data;
}

static uint32_t
pbus_pci_read(uint32_t addr, void *opaque)
{
	uint32_t *pci_conf = (uint32_t *)opaque;
	return pci_conf[(addr - NV_PBUS_PCI_BASE) / 4];
}

void
pbus_pci_init()
{
	void *pci_conf = pci_create_device(1, 0, 0, nv2a_pci_write);
	assert(pci_conf);
	pci_copy_default_configuration(pci_conf, (void *)default_pci_configuration, sizeof(default_pci_configuration));

	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, NV_PBUS_PCI_BASE, sizeof(default_pci_configuration), false,
		{ .fnr32 = pbus_pci_read, .fnw32 = pbus_pci_write }, pci_conf))) {
		throw nxbx_exp_abort("Failed to initialize pbus pci MMIO range");
	}
}
