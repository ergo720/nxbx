// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "pci.hpp"
#include "pmc.hpp"
#include "pbus.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include <cinttypes>
#include <cassert>

#define MODULE_NAME pbus


/** Private device implementation **/
class pbus::Impl
{
public:
	void init(cpu *cpu, nv2a *gpu, pci *pci);
	void reset();
	void updateIo() { updateIo(true); }
	template<bool log>
	uint32_t read32(uint32_t addr);
	template<bool log>
	void write32(uint32_t addr, const uint32_t value);
	template<bool log>
	uint32_t pciRead32(uint32_t addr);
	template<bool log>
	void pciWrite32(uint32_t addr, const uint32_t value);

private:
	void pciLogRead(uint32_t addr, uint32_t value);
	void pciLogWrite(uint32_t addr, uint32_t value);
	void updateIo(bool is_update);
	template<bool is_write, bool is_pci>
	auto getIoFunc(bool log, bool is_be);
	void pciInit();

	void *m_pci_conf;
	// connected devices
	pmc *m_pmc;
	pci *m_pci;
	cpu_t *m_lc86cpu;
	// registers
	uint32_t m_fbio_ram;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PBUS_FBIO_RAM, "NV_PBUS_FBIO_RAM" }
	};
};

// Values dumped from a Retail 1.0 xbox
static constexpr uint32_t s_default_pci_configuration[] = {
	0x02A010DE,
	0x02B00007,
	0x030000A1,
	0x0000F800,
	0xFD000000,
	0xF0000008,
	0x00000008,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000060,
	0x00000000,
	0x01050103,
	0x00000000,
	0x00200002,
	0x1F000017,
	0x1F000114,
	0x00000000,
	0x00000001,
	0x0023D6CE,
	0x0000000F,
	0x00024401,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x2B16D065,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

static int nv2aPciWrite(uint8_t *ptr, uint8_t addr, uint8_t value, void *opaque)
{
	return 0; // pass-through the write
}

template<bool log>
void pbus::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PBUS_FBIO_RAM:
		m_fbio_ram = value;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log>
uint32_t pbus::Impl::read32(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PBUS_FBIO_RAM:
		value = m_fbio_ram;
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool log>
void pbus::Impl::pciWrite32(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		pciLogWrite(addr, value);
	}

	uint32_t *pci_conf = (uint32_t *)m_pci_conf;
	pci_conf[(addr - NV_PBUS_PCI_BASE) / 4] = value;
}

template<bool log>
uint32_t pbus::Impl::pciRead32(uint32_t addr)
{
	uint32_t *pci_conf = (uint32_t *)m_pci_conf;
	uint32_t value = pci_conf[(addr - NV_PBUS_PCI_BASE) / 4];

	if constexpr (log) {
		pciLogRead(addr, value);
	}

	return value;
}

void pbus::Impl::pciLogRead(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pbus, false>("Read at NV_PBUS_PCI_NV_0 + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PBUS_PCI_BASE, addr, value);
}

void pbus::Impl::pciLogWrite(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pbus, false>("Write at NV_PBUS_PCI_NV_0 + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PBUS_PCI_BASE, addr, value);
}

void pbus::Impl::pciInit()
{
	void *pci_conf = m_pci->createDevice(1, 0, 0, nv2aPciWrite, nullptr);
	assert(pci_conf);
	m_pci->copyDefaultConfiguration(pci_conf, (void *)s_default_pci_configuration, sizeof(s_default_pci_configuration));
	m_pci_conf = pci_conf;
}

template<bool is_write, bool is_pci>
auto pbus::Impl::getIoFunc(bool log, bool is_be)
{
	if constexpr (is_pci) {
		if constexpr (is_write) {
			if (log) {
				return is_be ? nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::pciWrite32<true>, big> : nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::pciWrite32<true>, le>;
			}
			else {
				return is_be ? nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::pciWrite32<false>, big> : nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::pciWrite32<false>, le>;
			}
		}
		else {
			if (log) {
				return is_be ? nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::pciRead32<true>, big> : nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::pciRead32<true>, le>;
			}
			else {
				return is_be ? nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::pciRead32<false>, big> : nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::pciRead32<false>, le>;
			}
		}
	}
	else {
		if constexpr (is_write) {
			if (log) {
				return is_be ? nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::write32<true>, big> : nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::write32<true>, le>;
			}
			else {
				return is_be ? nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::write32<false>, big> : nv2a_write<pbus::Impl, uint32_t, &pbus::Impl::write32<false>, le>;
			}
		}
		else {
			if (log) {
				return is_be ? nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::read32<true>, big> : nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::read32<true>, le>;
			}
			else {
				return is_be ? nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::read32<false>, big> : nv2a_read<pbus::Impl, uint32_t, &pbus::Impl::read32<false>, le>;
			}
		}
	}
}

void pbus::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PBUS_BASE, NV_PBUS_SIZE, false,
		{
			.fnr32 = getIoFunc<false, false>(log, is_be),
			.fnw32 = getIoFunc<true, false>(log, is_be)
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update mmio region"));
	}

	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PBUS_PCI_BASE, sizeof(s_default_pci_configuration), false,
		{
			.fnr32 = getIoFunc<false, true>(log, is_be),
			.fnw32 = getIoFunc<true, true>(log, is_be)
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update pci mmio region"));
	}
}

void pbus::Impl::reset()
{
	// Values dumped from a Retail 1.0 xbox
	m_fbio_ram = 0x00010000 | NV_PBUS_FBIO_RAM_TYPE_DDR; // ddr even though is should be sdram?
}

void pbus::Impl::init(cpu *cpu, nv2a *gpu, pci *pci)
{
	m_pmc = gpu->getPmc();
	m_lc86cpu = cpu->get86cpu();
	m_pci = pci;
	pciInit();
	reset();
	updateIo(false);
}

/** Public interface implementation **/
void pbus::init(cpu *cpu, nv2a *gpu, pci *pci)
{
	m_impl->init(cpu, gpu, pci);
}

void pbus::reset()
{
	m_impl->reset();
}

void pbus::updateIo()
{
	m_impl->updateIo();
}

uint32_t pbus::read32(uint32_t addr)
{
	return m_impl->read32<false>(addr);
}

void pbus::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false>(addr, value);
}

uint32_t pbus::pciRead32(uint32_t addr)
{
	return m_impl->pciRead32<false>(addr);
}

void pbus::pciWrite32(uint32_t addr, const uint32_t value)
{
	m_impl->pciWrite32<false>(addr, value);
}

pbus::pbus() : m_impl{std::make_unique<pbus::Impl>()} {}
pbus::~pbus() {}
