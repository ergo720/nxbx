// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;
class ptimer;

class pramdac {
public:
	pramdac(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	uint8_t read8(uint32_t addr);
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t data);
	uint8_t read8_logger(uint32_t addr);
	uint32_t read32_logger(uint32_t addr);
	void write32_logger(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);

	friend class ptimer;
	machine *const m_machine;
	uint64_t core_freq; // gpu frequency
	struct {
		// core, memory and video clocks
		uint32_t nvpll_coeff, mpll_coeff, vpll_coeff;
	};
};
