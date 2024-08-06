// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PRAMDAC 0x00680300
#define NV_PRAMDAC_BASE (NV2A_REGISTER_BASE + NV_PRAMDAC)
#define NV_PRAMDAC_SIZE 0xD00

#define NV_PRAMDAC_NVPLL_COEFF (NV2A_REGISTER_BASE + 0x00680500) // core pll (phase-locked loop) coefficients
#define NV_PRAMDAC_NVPLL_COEFF_MDIV_MASK 0x000000FF
#define NV_PRAMDAC_NVPLL_COEFF_NDIV_MASK 0x0000FF00
#define NV_PRAMDAC_NVPLL_COEFF_PDIV_MASK 0x00070000
#define NV_PRAMDAC_MPLL_COEFF (NV2A_REGISTER_BASE + 0x00680504) // memory pll (phase-locked loop) coefficients
#define NV_PRAMDAC_VPLL_COEFF (NV2A_REGISTER_BASE + 0x00680508) // video pll (phase-locked loop) coefficients


class machine;
class ptimer;

class pramdac {
public:
	pramdac(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	uint32_t read32(uint32_t addr);
	template<bool log = false>
	void write32(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);
	template<bool is_write, typename T>
	auto get_io_func(bool log, bool is_be);

	friend class ptimer;
	machine *const m_machine;
	uint64_t core_freq; // gpu frequency
	// registers
	uint32_t nvpll_coeff, mpll_coeff, vpll_coeff;
};
