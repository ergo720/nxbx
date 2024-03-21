// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;

class pbus {
public:
	pbus(machine *machine) : m_machine(machine), m_pci_conf(nullptr) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	template<bool log = false>
	uint32_t read(uint32_t addr);
	template<bool log = false>
	void write(uint32_t addr, const uint32_t data);
	template<bool log = false>
	uint32_t pci_read(uint32_t addr);
	template<bool log = false>
	void pci_write(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);
	void pci_init();

	machine *const m_machine;
	void *m_pci_conf;
	struct {
		// Contains the ram type, among other unknown info about the ram modules
		uint32_t fbio_ram;
	};
};
