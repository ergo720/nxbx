// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PBUS 0x00001000
#define NV_PBUS_BASE (NV2A_REGISTER_BASE + NV_PBUS)
#define NV_PBUS_SIZE 0x1000
#define NV_PBUS_FBIO_RAM (NV2A_REGISTER_BASE + 0x00001218)
#define NV_PBUS_FBIO_RAM_TYPE_DDR (0x00000000 << 8)
#define NV_PBUS_FBIO_RAM_TYPE_SDR (0x00000001 << 8)
#define NV_PBUS_PCI_NV_0 (NV2A_REGISTER_BASE + 0x00001800)
#define NV_PBUS_PCI_BASE NV_PBUS_PCI_NV_0


class machine;

class pbus {
public:
	pbus(machine *machine) : m_machine(machine), m_pci_conf(nullptr) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false>
	uint32_t read32(uint32_t addr);
	template<bool log = false>
	void write32(uint32_t addr, const uint32_t data);
	template<bool log = false>
	uint32_t pci_read32(uint32_t addr);
	template<bool log = false>
	void pci_write32(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);
	template<bool is_write, bool is_pci>
	auto get_io_func(bool log, bool is_be);
	void pci_init();

	machine *const m_machine;
	void *m_pci_conf;
	// registers
	uint32_t fbio_ram; // Contains the ram type, among other unknown info about the ram modules
};
