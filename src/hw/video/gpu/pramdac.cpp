// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../../clock.hpp"
#include "machine.hpp"

#define MODULE_NAME pramdac


template<bool log>
void pramdac::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF: {
		// NOTE: if the m value is zero, then the final frequency is also zero
		nvpll_coeff = value;
		uint64_t m = value & NV_PRAMDAC_NVPLL_COEFF_MDIV;
		uint64_t n = (value & NV_PRAMDAC_NVPLL_COEFF_NDIV) >> 8;
		uint64_t p = (value & NV_PRAMDAC_NVPLL_COEFF_PDIV) >> 16;
		core_freq = m ? ((NV2A_CRYSTAL_FREQ * n) / (1ULL << p) / m) : 0;
		if (m_machine->get<ptimer>().counter_active) {
			m_machine->get<ptimer>().counter_period = m_machine->get<ptimer>().counter_to_us();
			cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(timer::get_now()));
		}
	}
	break;

	case NV_PRAMDAC_MPLL_COEFF:
		mpll_coeff = value;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		vpll_coeff = value;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log>
uint32_t pramdac::read32(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF:
		value = nvpll_coeff;
		break;

	case NV_PRAMDAC_MPLL_COEFF:
		value = mpll_coeff;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		value = vpll_coeff;
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
uint8_t pramdac::read8(uint32_t addr)
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
auto pramdac::get_io_func(bool log, bool is_be)
{
	if constexpr (is_write) {
		if (log) {
			return is_be ? nv2a_write<pramdac, T, &pramdac::write32<true>, big> : nv2a_write<pramdac, T, &pramdac::write32<true>, le>;
		}
		else {
			return is_be ? nv2a_write<pramdac, T, &pramdac::write32<false>, big> : nv2a_write<pramdac, T, &pramdac::write32<false>, le>;
		}
	}
	else {
		if constexpr (sizeof(T) == 1) {
			if (log) {
				return is_be ? nv2a_read<pramdac, T, &pramdac::read8<true>, big> : nv2a_read<pramdac, T, &pramdac::read8<true>, le>;
			}
			else {
				return is_be ? nv2a_read<pramdac, T, &pramdac::read8<false>, big> : nv2a_read<pramdac, T, &pramdac::read8<false>, le>;
			}
		}
		else {
			if (log) {
				return is_be ? nv2a_read<pramdac, T, &pramdac::read32<true>, big> : nv2a_read<pramdac, T, &pramdac::read32<true>, le>;
			}
			else {
				return is_be ? nv2a_read<pramdac, T, &pramdac::read32<false>, big> : nv2a_read<pramdac, T, &pramdac::read32<false>, le>;
			}
		}
	}
}

bool
pramdac::update_io(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRAMDAC_BASE, NV_PRAMDAC_SIZE, false,
		{
			.fnr8 = get_io_func<false, uint8_t>(log, is_be),
			.fnr32 = get_io_func<false, uint32_t>(log, is_be),
			.fnw32 = get_io_func<true, uint32_t>(log, is_be),
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pramdac::reset()
{
	// Values dumped from a Retail 1.0 xbox
	core_freq = NV2A_CLOCK_FREQ;
	nvpll_coeff = 0x00011C01;
	mpll_coeff = 0x00007702;
	vpll_coeff = 0x0003C20D;
}

bool
pramdac::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
