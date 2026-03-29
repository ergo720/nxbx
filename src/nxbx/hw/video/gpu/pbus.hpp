// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PBUS 0x00001000
#define NV_PBUS_BASE (NV2A_REGISTER_BASE + NV_PBUS)
#define NV_PBUS_SIZE 0x1000
#define NV_PBUS_FBIO_RAM (NV2A_REGISTER_BASE + 0x00001218) // Contains the ram type, among other unknown info about the ram modules
#define NV_PBUS_FBIO_RAM_TYPE_DDR (0x00000000 << 8)
#define NV_PBUS_FBIO_RAM_TYPE_SDR (0x00000001 << 8)
#define NV_PBUS_PCI_NV_0 (NV2A_REGISTER_BASE + 0x00001800)
#define NV_PBUS_PCI_BASE NV_PBUS_PCI_NV_0


class cpu;
class nv2a;
class pci;

class pbus {
public:
	pbus();
	~pbus();
	bool init(cpu *cpu, nv2a *gpu, pci *pci);
	void reset();
	void updateIo();
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);
	uint32_t pciRead32(uint32_t addr);
	void pciWrite32(uint32_t addr, const uint32_t value);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
