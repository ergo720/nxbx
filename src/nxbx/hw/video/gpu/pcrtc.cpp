// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "pmc.hpp"
#include "pcrtc.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include <cinttypes>

#define MODULE_NAME pcrtc


/** Private device implementation **/
class pcrtc::Impl
{
public:
	void init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo() { updateIo(true); }
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);

private:
	void updateIo(bool is_update);
	template<bool is_write>
	auto getIoFunc(bool log, bool enabled, bool is_be);

	// connected devices
	pmc *m_pmc;
	cpu_t *m_lc86cpu;
	// atomic registers
	std::atomic_uint32_t m_int_status;
	std::atomic_uint32_t m_int_enabled;
	// registers
	uint32_t m_fb_addr;
	uint32_t m_unknown;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PCRTC_INTR_0, "NV_PCRTC_INTR_0" },
		{ NV_PCRTC_INTR_EN_0, "NV_PCRTC_INTR_EN_0" },
		{ NV_PCRTC_START, "NV_PCRTC_START" },
		{ NV_PCRTC_UNKNOWN0, "NV_PCRTC_UNKNOWN0" },
	};
};

template<bool log, engine_enabled enabled>
void pcrtc::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PCRTC_INTR_0:
		m_int_status &= ~value;
		m_pmc->updateIrq();
		break;

	case NV_PCRTC_INTR_EN_0:
		m_int_enabled = value;
		m_pmc->updateIrq();
		break;

	case NV_PCRTC_START:
		m_fb_addr = value & 0x7FFFFFC; // fb is 4 byte aligned
		break;

	case NV_PCRTC_UNKNOWN0:
		m_unknown = value;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log, engine_enabled enabled>
uint32_t pcrtc::Impl::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value = 0;

	switch (addr)
	{
	case NV_PCRTC_INTR_0:
		value = m_int_status;
		break;

	case NV_PCRTC_INTR_EN_0:
		value = m_int_enabled;
		break;

	case NV_PCRTC_START:
		value = m_fb_addr;
		break;

	case NV_PCRTC_UNKNOWN0:
		value = m_unknown;
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool is_write>
auto pcrtc::Impl::getIoFunc(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pcrtc::Impl, uint32_t, &pcrtc::Impl::write32<true, on>, big> : nv2a_write<pcrtc::Impl, uint32_t, &pcrtc::Impl::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pcrtc::Impl, uint32_t, &pcrtc::Impl::write32<false, on>, big> : nv2a_write<pcrtc::Impl, uint32_t, &pcrtc::Impl::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pcrtc::Impl, uint32_t, &pcrtc::Impl::write32<false, off>, big>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pcrtc::Impl, uint32_t, &pcrtc::Impl::read32<true, on>, big> : nv2a_read<pcrtc::Impl, uint32_t, &pcrtc::Impl::read32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_read<pcrtc::Impl, uint32_t, &pcrtc::Impl::read32<false, on>, big> : nv2a_read<pcrtc::Impl, uint32_t, &pcrtc::Impl::read32<false, on>, le>;
			}
		}
		else {
			return nv2a_read<pcrtc::Impl, uint32_t, &pcrtc::Impl::read32<false, off>, big>;
		}
	}
}

void pcrtc::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_pmc->read32(NV_PMC_ENABLE) &NV_PMC_ENABLE_PCRTC;
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PCRTC_BASE, NV_PCRTC_SIZE, false,
		{
			.fnr32 = getIoFunc<false>(log, enabled, is_be),
			.fnw32 = getIoFunc<true>(log, enabled, is_be)
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update mmio region"));
	}
}

void pcrtc::Impl::reset()
{
	m_int_status = NV_PCRTC_INTR_0_VBLANK_NOT_PENDING;
	m_int_enabled = NV_PCRTC_INTR_EN_0_VBLANK_DISABLED;
	m_fb_addr = 0;
	m_unknown = 0;
}

void pcrtc::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_lc86cpu = cpu->get86cpu();
	reset();
	updateIo(false);
}

/** Public interface implementation **/
void pcrtc::init(cpu *cpu, nv2a *gpu)
{
	m_impl->init(cpu, gpu);
}

void pcrtc::reset()
{
	m_impl->reset();
}

void pcrtc::updateIo()
{
	m_impl->updateIo();
}

uint32_t pcrtc::read32(uint32_t addr)
{
	return m_impl->read32<false, on>(addr);
}

void pcrtc::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false, on>(addr, value);
}

pcrtc::pcrtc() : m_impl{std::make_unique<pcrtc::Impl>()} {}
pcrtc::~pcrtc() {}
