// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "clock.hpp"
#include "pmc.hpp"
#include "ptimer.hpp"
#include "pramdac.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include <cinttypes>

#define MODULE_NAME pramdac


/** Private device implementation **/
class pramdac::Impl
{
public:
	bool init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo() { updateIo(true); }
	template<bool log>
	uint8_t read8(uint32_t addr);
	template<bool log>
	uint32_t read32(uint32_t addr);
	template<bool log>
	void write32(uint32_t addr, const uint32_t value);
	uint64_t getCoreFreq() { return m_core_freq; }

private:
	bool updateIo(bool is_update);
	template<bool is_write, typename T>
	auto getIoFunc(bool log, bool is_be);

	uint64_t m_core_freq; // gpu frequency
	// connected devices
	pmc *m_pmc;
	ptimer *m_ptimer;
	cpu *m_cpu;
	cpu_t *m_lc86cpu;
	// registers
	uint32_t m_nvpll_coeff, m_mpll_coeff, m_vpll_coeff;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PRAMDAC_NVPLL_COEFF, "NV_PRAMDAC_NVPLL_COEFF" },
		{ NV_PRAMDAC_MPLL_COEFF, "NV_PRAMDAC_MPLL_COEFF" },
		{ NV_PRAMDAC_VPLL_COEFF, "NV_PRAMDAC_VPLL_COEFF" }
	};
};

template<bool log>
void pramdac::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF: {
		// NOTE: if the m value is zero, then the final frequency is also zero
		m_nvpll_coeff = value;
		uint64_t m = value & NV_PRAMDAC_NVPLL_COEFF_MDIV;
		uint64_t n = (value & NV_PRAMDAC_NVPLL_COEFF_NDIV) >> 8;
		uint64_t p = (value & NV_PRAMDAC_NVPLL_COEFF_PDIV) >> 16;
		m_core_freq = m ? ((NV2A_CRYSTAL_FREQ * n) / (1ULL << p) / m) : 0;
		if (m_ptimer->isCounterOn()) {
			m_ptimer->setCounterPeriod(m_ptimer->counterToUs());
			cpu_set_timeout(m_lc86cpu, m_cpu->checkPeriodicEvents(timer::get_now()));
		}
	}
	break;

	case NV_PRAMDAC_MPLL_COEFF:
		m_mpll_coeff = value;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		m_vpll_coeff = value;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log>
uint32_t pramdac::Impl::read32(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF:
		value = m_nvpll_coeff;
		break;

	case NV_PRAMDAC_MPLL_COEFF:
		value = m_mpll_coeff;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		value = m_vpll_coeff;
		break;

	default:
		nxbx_fatal("Unhandled %s read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool log>
uint8_t pramdac::Impl::read8(uint32_t addr)
{
	// This handler is necessary because Direct3D_CreateDevice reads the n value by accessing the second byte of the register, even though the coefficient
	// registers are supposed to be four bytes instead. This is probably due to compiler optimizations

	uint32_t addr_base = addr & ~3;
	uint32_t addr_offset = (addr & 3) << 3;
	uint32_t value32 = read32<false>(addr_base);
	uint8_t value = uint8_t((value32 & (0xFF << addr_offset)) >> addr_offset);

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool is_write, typename T>
auto pramdac::Impl::getIoFunc(bool log, bool is_be)
{
	if constexpr (is_write) {
		if (log) {
			return is_be ? nv2a_write<pramdac::Impl, T, &pramdac::Impl::write32<true>, big> : nv2a_write<pramdac::Impl, T, &pramdac::Impl::write32<true>, le>;
		}
		else {
			return is_be ? nv2a_write<pramdac::Impl, T, &pramdac::Impl::write32<false>, big> : nv2a_write<pramdac::Impl, T, &pramdac::Impl::write32<false>, le>;
		}
	}
	else {
		if constexpr (sizeof(T) == 1) {
			if (log) {
				return is_be ? nv2a_read<pramdac::Impl, T, &pramdac::Impl::read8<true>, big> : nv2a_read<pramdac::Impl, T, &pramdac::Impl::read8<true>, le>;
			}
			else {
				return is_be ? nv2a_read<pramdac::Impl, T, &pramdac::Impl::read8<false>, big> : nv2a_read<pramdac::Impl, T, &pramdac::Impl::read8<false>, le>;
			}
		}
		else {
			if (log) {
				return is_be ? nv2a_read<pramdac::Impl, T, &pramdac::Impl::read32<true>, big> : nv2a_read<pramdac::Impl, T, &pramdac::Impl::read32<true>, le>;
			}
			else {
				return is_be ? nv2a_read<pramdac::Impl, T, &pramdac::Impl::read32<false>, big> : nv2a_read<pramdac::Impl, T, &pramdac::Impl::read32<false>, le>;
			}
		}
	}
}

bool pramdac::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PRAMDAC_BASE, NV_PRAMDAC_SIZE, false,
		{
			.fnr8 = getIoFunc<false, uint8_t>(log, is_be),
			.fnr32 = getIoFunc<false, uint32_t>(log, is_be),
			.fnw32 = getIoFunc<true, uint32_t>(log, is_be),
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void pramdac::Impl::reset()
{
	// Values dumped from a Retail 1.0 xbox
	m_core_freq = NV2A_CLOCK_FREQ;
	m_nvpll_coeff = 0x00011C01;
	m_mpll_coeff = 0x00007702;
	m_vpll_coeff = 0x0003C20D;
}

bool pramdac::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_ptimer = gpu->getPtimer();
	m_lc86cpu = cpu->get86cpu();
	m_cpu = cpu;
	reset();

	if (!updateIo(false)) {
		return false;
	}

	return true;
}

/** Public interface implementation **/
bool pramdac::init(cpu *cpu, nv2a *gpu)
{
	return m_impl->init(cpu, gpu);
}

void pramdac::reset()
{
	m_impl->reset();
}

void pramdac::updateIo()
{
	m_impl->updateIo();
}

uint32_t pramdac::read32(uint32_t addr)
{
	return m_impl->read32<false>(addr);
}

void pramdac::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false>(addr, value);
}

uint64_t pramdac::getCoreFreq()
{
	return m_impl->getCoreFreq();
}

pramdac::pramdac() : m_impl{std::make_unique<pramdac::Impl>()} {}
pramdac::~pramdac() {}
