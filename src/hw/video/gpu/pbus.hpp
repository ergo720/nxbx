// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;
class pmc;
class pcrtc;
class pramdac;
class ptimer;
class pfb;
class pramin;
class pfifo;

class pbus {
public:
	pbus(machine *machine) : m_machine(machine), m_pci_conf(nullptr) {}
	bool init();
	void reset();
	constexpr const char *get_name() { return "NV2A.PBUS"; }
	uint32_t read(uint32_t addr);
	void write(uint32_t addr, const uint32_t data);
	uint32_t pci_read(uint32_t addr);
	void pci_write(uint32_t addr, const uint32_t data);

private:
	friend class pmc;
	friend class pcrtc;
	friend class pramdac;
	friend class ptimer;
	friend class pfb;
	friend class pramin;
	friend class pfifo;
	bool pci_init();

	machine *const m_machine;
	void *m_pci_conf;
	struct {
		// Contains the ram type, among other unknown info about the ram modules
		uint32_t fbio_ram;
	};
};
