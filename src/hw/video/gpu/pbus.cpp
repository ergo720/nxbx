// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define NV_PBUS 0x00001000
#define NV_PBUS_BASE (NV2A_REGISTER_BASE + NV_PBUS)
#define NV_PBUS_SIZE 0x1000
#define NV_PBUS_FBIO_RAM (NV2A_REGISTER_BASE + 0x00001218)
#define NV_PBUS_FBIO_RAM_TYPE_DDR (0x00000000 << 8)
#define NV_PBUS_FBIO_RAM_TYPE_SDR (0x00000001 << 8)
#define NV_PBUS_PCI_NV_0 (NV2A_REGISTER_BASE + 0x00001800)
#define NV_PBUS_PCI_BASE NV_PBUS_PCI_NV_0


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
nv2a_pci_write(uint8_t *ptr, uint8_t addr, uint8_t data, void *opaque)
{
	return 0; // pass-through the write
}

void
pbus::write(uint32_t addr, const uint32_t data)
{
	switch (addr)
	{
	case NV_PBUS_FBIO_RAM:
		fbio_ram = data;
		break;

	default:
		nxbx::fatal("Unhandled %s write at address 0x%" PRIX32 " with value 0x%" PRIX32, get_name(), addr, data);
	}
}

uint32_t
pbus::read(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PBUS_FBIO_RAM:
		value = fbio_ram;
		break;

	default:
		nxbx::fatal("Unhandled %s read at address 0x%" PRIX32, get_name(), addr);
	}

	return value;
}

void
pbus::pci_write(uint32_t addr, const uint32_t data)
{
	uint32_t *pci_conf = (uint32_t *)m_pci_conf;
	pci_conf[(addr - NV_PBUS_PCI_BASE) / 4] = data;
}

uint32_t
pbus::pci_read(uint32_t addr)
{
	uint32_t *pci_conf = (uint32_t *)m_pci_conf;
	return pci_conf[(addr - NV_PBUS_PCI_BASE) / 4];
}

bool
pbus::pci_init()
{
	void *pci_conf = m_machine->get<pci>().create_device(1, 0, 0, nv2a_pci_write, nullptr);
	assert(pci_conf);
	m_machine->get<pci>().copy_default_configuration(pci_conf, (void *)default_pci_configuration, sizeof(default_pci_configuration));

	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PBUS_PCI_BASE, sizeof(default_pci_configuration), false,
		{
			.fnr32 = cpu_read<pbus, uint32_t, &pbus::pci_read>,
			.fnw32 = cpu_write<pbus, uint32_t, &pbus::pci_write>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s (PCI) mmio ports", get_name());
		return false;
	}
	m_pci_conf = pci_conf;

	return true;
}

void
pbus::reset()
{
	// Values dumped from a Retail 1.0 xbox
	fbio_ram = 0x00010000 | NV_PBUS_FBIO_RAM_TYPE_DDR; // ddr even though is should be sdram?
}

bool
pbus::init()
{
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PBUS_BASE, NV_PBUS_SIZE, false,
		{
			.fnr32 = cpu_read<pbus, uint32_t, &pbus::read>,
			.fnw32 = cpu_write<pbus, uint32_t, &pbus::write>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio ports", get_name());
		return false;
	}

	reset();
	return pci_init();
}
