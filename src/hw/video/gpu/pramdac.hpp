// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;
class pmc;
class pcrtc;
class ptimer;
class pfb;
class pbus;
class pramin;
class pfifo;

class pramdac {
public:
	pramdac(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	constexpr const char *get_name() { return "NV2A.PRAMDAC"; }
	uint8_t read8(uint32_t addr);
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t data);

private:
	friend class pmc;
	friend class pcrtc;
	friend class ptimer;
	friend class pfb;
	friend class pbus;
	friend class pramin;
	friend class pfifo;
	machine *const m_machine;
	uint64_t core_freq; // gpu frequency
	struct {
		// core, memory and video clocks
		uint32_t nvpll_coeff, mpll_coeff, vpll_coeff;
	};
};
