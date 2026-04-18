// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PMC 0x00000000
#define NV_PMC_BASE (NV2A_REGISTER_BASE + NV_PMC)
#define NV_PMC_SIZE 0x1000

#define NV_PMC_BOOT_0 (NV2A_REGISTER_BASE + 0x00000000) // Contains the gpu identification number
#define NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0 0x02A000A3
#define NV_PMC_BOOT_1 (NV2A_REGISTER_BASE + 0x00000004) // Switches the endianness of all accesses done through BAR0
#define NV_PMC_BOOT_1_ENDIAN0_LITTLE (0x00000000 << 0)
#define NV_PMC_BOOT_1_ENDIAN0_BIG (0x00000001 << 0)
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE (0x00000000 << 24)
#define NV_PMC_BOOT_1_ENDIAN24_BIG (0x00000001 << 24)
#define NV_PMC_INTR_0 (NV2A_REGISTER_BASE + 0x00000100) // Pending interrupts of all engines
#define NV_PMC_INTR_0_PFIFO 8
#define NV_PMC_INTR_0_PGRAPH 12
#define NV_PMC_INTR_0_PTIMER 20
#define NV_PMC_INTR_0_PCRTC 24
#define NV_PMC_INTR_0_SOFTWARE 31
#define NV_PMC_INTR_0_NOT_PENDING 0x00000000
#define NV_PMC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00000140) // Enable/disable hw/sw interrupts
#define NV_PMC_INTR_EN_0_INTA_DISABLED 0x00000000
#define NV_PMC_INTR_EN_0_INTA_HARDWARE 0x00000001
#define NV_PMC_INTR_EN_0_INTA_SOFTWARE 0x00000002
#define NV_PMC_ENABLE (NV2A_REGISTER_BASE + 0x00000200) // Enable/disable gpu engines
#define NV_PMC_ENABLE_PFIFO (1 << 8)
#define NV_PMC_ENABLE_PGRAPH (1 << 12)
#define NV_PMC_ENABLE_PTIMER (1 << 16)
#define NV_PMC_ENABLE_PFB (1 << 20)
#define NV_PMC_ENABLE_PCRTC (1 << 24)
#define NV_PMC_ENABLE_PVIDEO (1 << 28)
#define NV_PMC_ENABLE_ALL (NV_PMC_ENABLE_PFIFO | NV_PMC_ENABLE_PGRAPH | NV_PMC_ENABLE_PTIMER | NV_PMC_ENABLE_PFB | NV_PMC_ENABLE_PCRTC | NV_PMC_ENABLE_PVIDEO)


class cpu;
class nv2a;
class machine;

class pmc
{
public:
	pmc();
	~pmc();
	void init(cpu *cpu, nv2a *gpu, machine *machine);
	void reset();
	void updateIo();
	void updateIrq();
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
