// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PUSER 0x00800000
#define NV_PUSER_BASE (NV2A_REGISTER_BASE + NV_PUSER)
#define NV_PUSER_SIZE 0x800000

#define NV_PUSER_DMA_PUT (NV2A_REGISTER_BASE + 0x00800040)
#define NV_PUSER_DMA_GET (NV2A_REGISTER_BASE + 0x00800044)
#define NV_PUSER_REF (NV2A_REGISTER_BASE + 0x00800048)


class cpu;
class nv2a;

class puser
{
public:
	puser();
	~puser();
	void init(cpu *cpu, nv2a *gpu);
	void updateIo();
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
