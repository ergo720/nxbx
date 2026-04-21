// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "pmc.hpp"
#include "pvideo.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include <cinttypes>

#define MODULE_NAME pvideo


/** Private device implementation **/
class pvideo::Impl
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
	uint32_t debug[11];
	uint32_t m_regs[24];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PVIDEO_DEBUG_0, "NV_PVIDEO_DEBUG_0" },
		{ NV_PVIDEO_DEBUG_1, "NV_PVIDEO_DEBUG_1" },
		{ NV_PVIDEO_DEBUG_2, "NV_PVIDEO_DEBUG_2" },
		{ NV_PVIDEO_DEBUG_3, "NV_PVIDEO_DEBUG_3" },
		{ NV_PVIDEO_DEBUG_4, "NV_PVIDEO_DEBUG_4" },
		{ NV_PVIDEO_DEBUG_5, "NV_PVIDEO_DEBUG_5" },
		{ NV_PVIDEO_DEBUG_6, "NV_PVIDEO_DEBUG_6" },
		{ NV_PVIDEO_DEBUG_7, "NV_PVIDEO_DEBUG_7" },
		{ NV_PVIDEO_DEBUG_8, "NV_PVIDEO_DEBUG_8" },
		{ NV_PVIDEO_DEBUG_9, "NV_PVIDEO_DEBUG_9" },
		{ NV_PVIDEO_DEBUG_10, "NV_PVIDEO_DEBUG_10" },
		{ NV_PVIDEO_LUMINANCE(0), "NV_PVIDEO_LUMINANCE(0)" },
		{ NV_PVIDEO_LUMINANCE(1), "NV_PVIDEO_LUMINANCE(1)" },
		{ NV_PVIDEO_CHROMINANCE(0),"NV_PVIDEO_CHROMINANCE(0)" },
		{ NV_PVIDEO_CHROMINANCE(1), "NV_PVIDEO_CHROMINANCE(1)" },
		{ NV_PVIDEO_SIZE_IN(0), "NV_PVIDEO_SIZE_IN(0)" },
		{ NV_PVIDEO_SIZE_IN(1), "NV_PVIDEO_SIZE_IN(1)" },
		{ NV_PVIDEO_POINT_IN(0), "NV_PVIDEO_POINT_IN(0)" },
		{ NV_PVIDEO_POINT_IN(1), "NV_PVIDEO_POINT_IN(1)" },
		{ NV_PVIDEO_DS_DX(0), "NV_PVIDEO_DS_DX(0)" },
		{ NV_PVIDEO_DS_DX(1), "NV_PVIDEO_DS_DX(1)" },
		{ NV_PVIDEO_DT_DY(0), "NV_PVIDEO_DT_DY(0)" },
		{ NV_PVIDEO_DT_DY(1), "NV_PVIDEO_DT_DY(1)" },
	};
};

template<bool log, engine_enabled enabled>
void pvideo::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PVIDEO_DEBUG_0:
	case NV_PVIDEO_DEBUG_1:
	case NV_PVIDEO_DEBUG_2:
	case NV_PVIDEO_DEBUG_3:
	case NV_PVIDEO_DEBUG_4:
	case NV_PVIDEO_DEBUG_5:
	case NV_PVIDEO_DEBUG_6:
	case NV_PVIDEO_DEBUG_7:
	case NV_PVIDEO_DEBUG_8:
	case NV_PVIDEO_DEBUG_9:
	case NV_PVIDEO_DEBUG_10:
		debug[(addr - NV_PVIDEO_DEBUG_0) >> 2] = value;
		break;

	case NV_PVIDEO_INTR:
		m_int_status &= ~value;
		m_pmc->updateIrq();
		break;

	case NV_PVIDEO_INTR_EN:
		m_int_enabled = value;
		m_pmc->updateIrq();
		break;

	case NV_PVIDEO_LUMINANCE(0):
	case NV_PVIDEO_LUMINANCE(1):
	case NV_PVIDEO_CHROMINANCE(0):
	case NV_PVIDEO_CHROMINANCE(1):
	case NV_PVIDEO_SIZE_IN(0):
	case NV_PVIDEO_SIZE_IN(1):
	case NV_PVIDEO_POINT_IN(0):
	case NV_PVIDEO_POINT_IN(1):
	case NV_PVIDEO_DS_DX(0):
	case NV_PVIDEO_DS_DX(1):
	case NV_PVIDEO_DT_DY(0):
	case NV_PVIDEO_DT_DY(1):
		m_regs[(addr - NV_PVIDEO_BASE(0)) >> 2] = value;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log, engine_enabled enabled>
uint32_t pvideo::Impl::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value = 0;

	switch (addr)
	{
	case NV_PVIDEO_DEBUG_0:
	case NV_PVIDEO_DEBUG_1:
	case NV_PVIDEO_DEBUG_2:
	case NV_PVIDEO_DEBUG_3:
	case NV_PVIDEO_DEBUG_4:
	case NV_PVIDEO_DEBUG_5:
	case NV_PVIDEO_DEBUG_6:
	case NV_PVIDEO_DEBUG_7:
	case NV_PVIDEO_DEBUG_8:
	case NV_PVIDEO_DEBUG_9:
	case NV_PVIDEO_DEBUG_10:
		value = debug[(addr - NV_PVIDEO_DEBUG_0) >> 2];
		break;

	case NV_PVIDEO_INTR:
		value = m_int_status;
		break;

	case NV_PVIDEO_INTR_EN:
		value = m_int_enabled;
		break;

	case NV_PVIDEO_LUMINANCE(0):
	case NV_PVIDEO_LUMINANCE(1):
	case NV_PVIDEO_CHROMINANCE(0):
	case NV_PVIDEO_CHROMINANCE(1):
	case NV_PVIDEO_SIZE_IN(0):
	case NV_PVIDEO_SIZE_IN(1):
	case NV_PVIDEO_POINT_IN(0):
	case NV_PVIDEO_POINT_IN(1):
	case NV_PVIDEO_DS_DX(0):
	case NV_PVIDEO_DS_DX(1):
	case NV_PVIDEO_DT_DY(0):
	case NV_PVIDEO_DT_DY(1):
		value = m_regs[(addr - NV_PVIDEO_BASE(0)) >> 2];
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
auto pvideo::Impl::getIoFunc(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pvideo::Impl, uint32_t, &pvideo::Impl::write32<true, on>, big> : nv2a_write<pvideo::Impl, uint32_t, &pvideo::Impl::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pvideo::Impl, uint32_t, &pvideo::Impl::write32<false, on>, big> : nv2a_write<pvideo::Impl, uint32_t, &pvideo::Impl::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pvideo::Impl, uint32_t, &pvideo::Impl::write32<false, off>, big>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pvideo::Impl, uint32_t, &pvideo::Impl::read32<true, on>, big> : nv2a_read<pvideo::Impl, uint32_t, &pvideo::Impl::read32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_read<pvideo::Impl, uint32_t, &pvideo::Impl::read32<false, on>, big> : nv2a_read<pvideo::Impl, uint32_t, &pvideo::Impl::read32<false, on>, le>;
			}
		}
		else {
			return nv2a_read<pvideo::Impl, uint32_t, &pvideo::Impl::read32<false, off>, big>;
		}
	}
}

void pvideo::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_pmc->read32(NV_PMC_ENABLE) & NV_PMC_ENABLE_PVIDEO;
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PVIDEO_MMIO_BASE, NV_PVIDEO_SIZE, false,
		{
			.fnr32 = getIoFunc<false>(log, enabled, is_be),
			.fnw32 = getIoFunc<true>(log, enabled, is_be)
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update mmio region"));
	}
}

void
pvideo::Impl::reset()
{
	m_int_status = 0;
	m_int_enabled = 0;
	// Values dumped from a Retail 1.0 xbox
	debug[0] = 0x00000010;
	debug[1] = 0x00000064;
	debug[2] = 0x04000200;
	debug[3] = 0x03B004B0;
	debug[4] = 0x0016A0A0;
	debug[5] = 0x00188160;
	debug[6] = 0x0012C730;
	debug[7] = 0x00000000;
	debug[8] = 0x000000B0;
	debug[9] = 0x00000000;
	debug[10] = 0x0010026C;
}

void pvideo::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_lc86cpu = cpu->get86cpu();
	reset();
	updateIo(false);
}

/** Public interface implementation **/
void pvideo::init(cpu *cpu, nv2a *gpu)
{
	m_impl->init(cpu, gpu);
}

void pvideo::reset()
{
	m_impl->reset();
}

void pvideo::updateIo()
{
	m_impl->updateIo();
}

uint32_t pvideo::read32(uint32_t addr)
{
	return m_impl->read32<false, on>(addr);
}

void pvideo::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false, on>(addr, value);
}

pvideo::pvideo() : m_impl{std::make_unique<pvideo::Impl>()} {}
pvideo::~pvideo() {}

