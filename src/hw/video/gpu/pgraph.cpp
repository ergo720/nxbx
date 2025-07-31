// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2025 ergo720

#include "machine.hpp"

#define MODULE_NAME pgraph


template<bool log, engine_enabled enabled>
void pgraph::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PGRAPH_INTR:
		REG_PGRAPH(addr) &= ~value;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PGRAPH_INTR_EN:
		REG_PGRAPH(addr) = value;
		m_machine->get<pmc>().update_irq();
		break;

	default:
		REG_PGRAPH(addr) = value;
	}
}

template<bool log, engine_enabled enabled>
uint32_t pgraph::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value = REG_PGRAPH(addr);

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool is_write>
auto pgraph::get_io_func(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pgraph, uint32_t, &pgraph::write32<true, on>, big> : nv2a_write<pgraph, uint32_t, &pgraph::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pgraph, uint32_t, &pgraph::write32<false, on>, big> : nv2a_write<pgraph, uint32_t, &pgraph::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pgraph, uint32_t, &pgraph::write32<false, off>, big>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pgraph, uint32_t, &pgraph::read32<true, on>, big> : nv2a_read<pgraph, uint32_t, &pgraph::read32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_read<pgraph, uint32_t, &pgraph::read32<false, on>, big> : nv2a_read<pgraph, uint32_t, &pgraph::read32<false, on>, le>;
			}
		}
		else {
			return nv2a_read<pgraph, uint32_t, &pgraph::read32<false, off>, big>;
		}
	}
}

bool
pgraph::update_io(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_machine->get<pmc>().engine_enabled & NV_PMC_ENABLE_PGRAPH;
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PGRAPH_BASE, NV_PGRAPH_SIZE, false,
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
pgraph::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
}

bool
pgraph::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
