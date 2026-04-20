// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PCRTC 0x00600000
#define NV_PCRTC_BASE (NV2A_REGISTER_BASE + NV_PCRTC)
#define NV_PCRTC_SIZE 0x1000

#define NV_PCRTC_INTR_0 (NV2A_REGISTER_BASE + 0x00600100) // Pending vblank interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PCRTC_INTR_0_VBLANK_NOT_PENDING 0x00000000
#define NV_PCRTC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00600140) // Enable/disable vblank interrupt
#define NV_PCRTC_INTR_EN_0_VBLANK_DISABLED 0x00000000
#define NV_PCRTC_START (NV2A_REGISTER_BASE + 0x00600800) // The address of the framebuffer
#define NV_PCRTC_CONFIG (NV2A_REGISTER_BASE + 0x00600804) // Unknown


class cpu;
class nv2a;

class pcrtc
{
public:
	pcrtc();
	~pcrtc();
	void init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo();
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
