/*
* SPDX-License-Identifier: GPL-3.0-only
*
* ergo720                Copyright (c) 2026
*
*  This program is free software: you can redistribute it and/or modify it under the terms of the
*  GNU General Public License as published by the Free Software Foundation, version 3.
*
*  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*  See the GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along with this program.
*  If not, see <https://www.gnu.org/licenses/>. 
*/

#include "machine.hpp"
#include "hostbridge.hpp"
#include "cpu.hpp"
#include "pci.hpp"
#include <cinttypes>

#define MODULE_NAME hostbridge

#define CPU_MAX_MEM        0x84
#define CPU_MAX_MEM_64MB   0x03
#define CPU_MAX_MEM_128MB  0x07
#define CPU_MAX_MEM_256MB  0x0F
#define CPU_MAX_MEM_RESV   0xFFFFFF


/** Private device implementation **/
class hostbridge::Impl
{
public:
	void init(machine *machine);
	void reset();

private:
	void pciInit();
	void setMemorySize();

	uint32_t m_ramsize;
	uint8_t *m_pci_conf;
	// connected devices
	pci *m_pci;
};

// Values dumped from a Retail 1.0 xbox
static constexpr uint32_t s_default_pci_configuration[] = {
	0x02A510DE,
	0x00100006,
	0x060000A1,
	0x00800000,
	0x40000008,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000040,
	0x00000000,
	0x00000000,
	0x00206002,
	0x1F000217,
	0x00000014,
	0x00000001,
	0x00000000,
	0x10000000,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0x20010008,
	0x88880120,
	0x00000010,
	0x0F0F0F21,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0x00000100,
	0x03FFFFFF,
	0x00000001,
	0x3FF6F417,
	0x00000000,
	0xFB5AADFB,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000200,
	0x00000000,
	0x00000000,
	0x00000001,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00033333,
	0x00033333,
	0x00000013,
	0x00000000,
	0x00000000,
	0x00000001,
	0x07F0FFFF,
	0x00000000,
	0x00400006,
	0x01FF75B7,
	0x00000000,
	0x00000000,
	0xF0000001,
	0x00000000,
	0x00000000,
	0x00000000,
};

static int hostBridgePciWrite(uint8_t *ptr, uint8_t addr, uint8_t value, void *opaque)
{
	return 0; // pass-through the write
}

void hostbridge::Impl::setMemorySize()
{
	uint32_t reg_off = CPU_MAX_MEM + 3;
	uint32_t val = (m_ramsize >> 24) - 1; // memory is counted in increments of 16 MiB each
	m_pci_conf[reg_off] = val;
}

void hostbridge::Impl::pciInit()
{
	void *pci_conf = m_pci->createDevice(0, 0, 0, hostBridgePciWrite, nullptr);
	m_pci->copyDefaultConfiguration(pci_conf, (void *)s_default_pci_configuration, sizeof(s_default_pci_configuration));
	m_pci_conf = static_cast<uint8_t *>(pci_conf);
	setMemorySize();
}

void hostbridge::Impl::reset()
{
	m_pci->copyDefaultConfiguration(m_pci_conf, (void *)s_default_pci_configuration, sizeof(s_default_pci_configuration));
	setMemorySize();
}

void hostbridge::Impl::init(machine *machine)
{
	m_pci = machine->getPci();
	m_ramsize = machine->getCpu()->getRamsize();
	pciInit();
}

/** Public interface implementation **/
void hostbridge::init(machine *machine)
{
	m_impl->init(machine);
}

void hostbridge::reset()
{
	m_impl->reset();
}

hostbridge::hostbridge() : m_impl{std::make_unique<hostbridge::Impl>()} {}
hostbridge::~hostbridge() {}
