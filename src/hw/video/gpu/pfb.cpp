// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pfb


template<bool log, bool enabled>
void pfb::write32(uint32_t addr, const uint32_t value)
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

template<bool log, bool enabled>
uint32_t pfb::read32(uint32_t addr)
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
auto pfb::get_io_func(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pfb, uint32_t, &pfb::write32<true, true>, true> : nv2a_write<pfb, uint32_t, &pfb::write32<true>>;
			}
			else {
				return is_be ? nv2a_write<pfb, uint32_t, &pfb::write32<false, true>, true> : nv2a_write<pfb, uint32_t, &pfb::write32<false>>;
			}
		}
		else {
			return nv2a_write<pfb, uint32_t, &pfb::write32<false, false>>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pfb, uint32_t, &pfb::read32<true, true>, true> : nv2a_read<pfb, uint32_t, &pfb::read32<true>>;
			}
			else {
				return is_be ? nv2a_read<pfb, uint32_t, &pfb::read32<false, true>, true> : nv2a_read<pfb, uint32_t, &pfb::read32<false>>;
			}
		}
		else {
			return nv2a_read<pfb, uint32_t, &pfb::read32<false, false>>;
		}
	}
}

bool
pfb::update_io(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_machine->get<pmc>().engine_enabled & NV_PMC_ENABLE_PFB;
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFB_BASE, NV_PFB_SIZE, false,
		{
			.fnr32 = get_io_func<false>(log, enabled, is_be),
			.fnw32 = get_io_func<true>(log, enabled, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pfb::reset()
{
	// Values dumped from a Retail 1.0 xbox
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_regs[REGS_PFB_idx(NV_PFB_CFG0)] = 0x03070003;
	m_regs[REGS_PFB_idx(NV_PFB_CFG1)] = 0x11448000;
	m_regs[REGS_PFB_idx(NV_PFB_CSTATUS)] = m_machine->get<cpu>().get_ramsize();
}

bool
pfb::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
