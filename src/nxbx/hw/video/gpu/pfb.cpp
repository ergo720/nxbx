// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.h"
#include "pfb.hpp"
#include "pmc.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"

#define MODULE_NAME pfb


/** Private device implementation **/
class pfb::Impl
{
public:
	bool init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo() { updateIo(true); }
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);

private:
	bool updateIo(bool is_update);
	template<bool is_write>
	auto getIoFunc(bool log, bool enabled, bool is_be);

	// connected devices
	pmc *m_pmc;
	cpu *m_cpu;
	cpu_t *m_lc86cpu;
	// registers
	uint32_t m_regs[NV_PFB_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PFB_CFG0, "NV_PFB_CFG0" },
		{ NV_PFB_CFG1, "NV_PFB_CFG1" },
		{ NV_PFB_CSTATUS, "NV_PFB_CSTATUS" },
		{ NV_PFB_NVM, "NV_PFB_NVM" },
		{ NV_PFB_WBC, "NV_PFB_WBC" },
	};
};

template<bool log, engine_enabled enabled>
void pfb::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PFB_CSTATUS:
		// This register is read-only
		break;

	case NV_PFB_WBC:
		// Mask out the flush pending bit, to always report it as not pending
		m_regs[REGS_PFB_idx(NV_PFB_WBC)] = value & ~NV_PFB_WBC_FLUSH;
		break;

	default:
		m_regs[REGS_PFB_idx(addr)] = value;
	}
}

template<bool log, engine_enabled enabled>
uint32_t pfb::Impl::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value = m_regs[REGS_PFB_idx(addr)];

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool is_write>
auto pfb::Impl::getIoFunc(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pfb::Impl, uint32_t, &pfb::Impl::write32<true, on>, big> : nv2a_write<pfb::Impl, uint32_t, &pfb::Impl::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pfb::Impl, uint32_t, &pfb::Impl::write32<false, on>, big> : nv2a_write<pfb::Impl, uint32_t, &pfb::Impl::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pfb::Impl, uint32_t, &pfb::Impl::write32<false, off>, big>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pfb::Impl, uint32_t, &pfb::Impl::read32<true, on>, big> : nv2a_read<pfb::Impl, uint32_t, &pfb::Impl::read32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_read<pfb::Impl, uint32_t, &pfb::Impl::read32<false, on>, big> : nv2a_read<pfb::Impl, uint32_t, &pfb::Impl::read32<false, on>, le>;
			}
		}
		else {
			return nv2a_read<pfb::Impl, uint32_t, &pfb::Impl::read32<false, off>, big>;
		}
	}
}

bool pfb::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_pmc->read32(NV_PMC_ENABLE) & NV_PMC_ENABLE_PFB;
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PFB_BASE, NV_PFB_SIZE, false,
		{
			.fnr32 = getIoFunc<false>(log, enabled, is_be),
			.fnw32 = getIoFunc<true>(log, enabled, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void pfb::Impl::reset()
{
	// Values dumped from a Retail 1.0 xbox
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_regs[REGS_PFB_idx(NV_PFB_CFG0)] = 0x03070003;
	m_regs[REGS_PFB_idx(NV_PFB_CFG1)] = 0x11448000;
	m_regs[REGS_PFB_idx(NV_PFB_CSTATUS)] = m_cpu->getRamsize();
}

bool pfb::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_lc86cpu = cpu->get86cpu();
	m_cpu = cpu;
	reset();

	if (!updateIo(false)) {
		return false;
	}

	return true;
}

/** Public interface implementation **/
bool pfb::init(cpu *cpu, nv2a *gpu)
{
	return m_impl->init(cpu, gpu);
}

void pfb::reset()
{
	m_impl->reset();
}

void pfb::updateIo()
{
	m_impl->updateIo();
}

uint32_t pfb::read32(uint32_t addr)
{
	return m_impl->read32<false, on>(addr);
}

void pfb::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false, on>(addr, value);
}

pfb::pfb() : m_impl{std::make_unique<pfb::Impl>()} {}
pfb::~pfb() {}
