// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;

class pfifo {
public:
	pfifo(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	constexpr const char *get_name() { return "NV2A.PFIFO"; }
	uint32_t read(uint32_t addr);
	void write(uint32_t addr, const uint32_t data);

private:
	machine *const m_machine;
	struct {
		// Contain the base address and size of ramht, ramfc and ramro in ramin
		uint32_t ramht, ramfc, ramro;
	};
};
