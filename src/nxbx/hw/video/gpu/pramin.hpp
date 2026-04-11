// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PRAMIN 0x00700000
#define NV_PRAMIN_BASE (NV2A_REGISTER_BASE + NV_PRAMIN)
#define NV_PRAMIN_SIZE 0x100000 // = 1 MiB


class cpu;
class nv2a;

class pramin
{
public:
	pramin();
	~pramin();
	bool init(cpu *cpu, nv2a *gpu);
	void updateIo();
	uint32_t read32(uint32_t offset);
	void write32(uint32_t offset, const uint32_t value);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
