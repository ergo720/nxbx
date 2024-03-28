// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pbus


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

template<bool log>
void pbus::write(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	switch (addr)
	{
	case NV_PBUS_FBIO_RAM:
		fbio_ram = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log>
uint32_t pbus::read(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PBUS_FBIO_RAM:
		value = fbio_ram;
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void pbus::pci_write(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	uint32_t *pci_conf = (uint32_t *)m_pci_conf;
	pci_conf[(addr - NV_PBUS_PCI_BASE) / 4] = data;
}

template<bool log>
uint32_t pbus::pci_read(uint32_t addr)
{
	uint32_t *pci_conf = (uint32_t *)m_pci_conf;
	uint32_t value = pci_conf[(addr - NV_PBUS_PCI_BASE) / 4];

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

void
pbus::pci_init()
{
	void *pci_conf = m_machine->get<pci>().create_device(1, 0, 0, nv2a_pci_write, nullptr);
	assert(pci_conf);
	m_machine->get<pci>().copy_default_configuration(pci_conf, (void *)default_pci_configuration, sizeof(default_pci_configuration));
	m_pci_conf = pci_conf;
}

template<bool is_write, bool is_pci>
auto pbus::get_io_func(bool log, bool is_be)
{
	if constexpr (is_pci) {
		if constexpr (is_write) {
			if (log) {
				return is_be ? nv2a_write<pbus, uint32_t, &pbus::pci_write<true>, true> : nv2a_write<pbus, uint32_t, &pbus::pci_write<true>>;
			}
			else {
				return is_be ? nv2a_write<pbus, uint32_t, &pbus::pci_write<false>, true> : nv2a_write<pbus, uint32_t, &pbus::pci_write<false>>;
			}
		}
		else {
			if (log) {
				return is_be ? nv2a_read<pbus, uint32_t, &pbus::pci_read<true>, true> : nv2a_read<pbus, uint32_t, &pbus::pci_read<true>>;
			}
			else {
				return is_be ? nv2a_read<pbus, uint32_t, &pbus::pci_read<false>, true> : nv2a_read<pbus, uint32_t, &pbus::pci_read<false>>;
			}
		}
	}
	else {
		if constexpr (is_write) {
			if (log) {
				return is_be ? nv2a_write<pbus, uint32_t, &pbus::write<true>, true> : nv2a_write<pbus, uint32_t, &pbus::write<true>>;
			}
			else {
				return is_be ? nv2a_write<pbus, uint32_t, &pbus::write<false>, true> : nv2a_write<pbus, uint32_t, &pbus::write<false>>;
			}
		}
		else {
			if (log) {
				return is_be ? nv2a_read<pbus, uint32_t, &pbus::read<true>, true> : nv2a_read<pbus, uint32_t, &pbus::read<true>>;
			}
			else {
				return is_be ? nv2a_read<pbus, uint32_t, &pbus::read<false>, true> : nv2a_read<pbus, uint32_t, &pbus::read<false>>;
			}
		}
	}
}

bool
pbus::update_io(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PBUS_BASE, NV_PBUS_SIZE, false,
		{
			.fnr32 = get_io_func<false, false>(log, is_be),
			.fnw32 = get_io_func<true, false>(log, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PBUS_PCI_BASE, sizeof(default_pci_configuration), false,
		{
			.fnr32 = get_io_func<false, true>(log, is_be),
			.fnw32 = get_io_func<true, true>(log, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update pci mmio region");
		return false;
	}

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
	if (!update_io(false)) {
		return false;
	}

	pci_init();
	reset();
	return true;
}
