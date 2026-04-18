// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PRAMDAC 0x00680300
#define NV_PRAMDAC_BASE (NV2A_REGISTER_BASE + NV_PRAMDAC)
#define NV_PRAMDAC_SIZE 0xD00

#define NV_PRAMDAC_NVPLL_COEFF (NV2A_REGISTER_BASE + 0x00680500) // core pll (phase-locked loop) coefficients
#define NV_PRAMDAC_NVPLL_COEFF_MDIV 0x000000FF
#define NV_PRAMDAC_NVPLL_COEFF_NDIV 0x0000FF00
#define NV_PRAMDAC_NVPLL_COEFF_PDIV 0x00070000
#define NV_PRAMDAC_MPLL_COEFF (NV2A_REGISTER_BASE + 0x00680504) // memory pll (phase-locked loop) coefficients
#define NV_PRAMDAC_VPLL_COEFF (NV2A_REGISTER_BASE + 0x00680508) // video pll (phase-locked loop) coefficients


class cpu;
class nv2a;

class pramdac
{
public:
	pramdac();
	~pramdac();
	void init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo();
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);
	uint64_t getCoreFreq();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
