// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pvideo


template<bool log, bool enabled>
void pvideo::write32(uint32_t addr, const uint32_t data)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		log_io_write();
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
		debug[(addr - NV_PVIDEO_DEBUG_0) >> 2] = data;
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
		regs[(addr - NV_PVIDEO_BASE(0)) >> 2] = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log, bool enabled>
uint32_t pvideo::read32(uint32_t addr)
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
		value = regs[(addr - NV_PVIDEO_BASE(0)) >> 2];
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool is_write>
auto pvideo::get_io_func(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pvideo, uint32_t, &pvideo::write32<true, true>, true> : nv2a_write<pvideo, uint32_t, &pvideo::write32<true>>;
			}
			else {
				return is_be ? nv2a_write<pvideo, uint32_t, &pvideo::write32<false, true>, true> : nv2a_write<pvideo, uint32_t, &pvideo::write32<false>>;
			}
		}
		else {
			return nv2a_write<pvideo, uint32_t, &pvideo::write32<false, false>>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pvideo, uint32_t, &pvideo::read32<true, true>, true> : nv2a_read<pvideo, uint32_t, &pvideo::read32<true>>;
			}
			else {
				return is_be ? nv2a_read<pvideo, uint32_t, &pvideo::read32<false, true>, true> : nv2a_read<pvideo, uint32_t, &pvideo::read32<false>>;
			}
		}
		else {
			return nv2a_read<pvideo, uint32_t, &pvideo::read32<false, false>>;
		}
	}
}

bool
pvideo::update_io(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_machine->get<pmc>().engine_enabled & NV_PMC_ENABLE_PVIDEO;
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PVIDEO_MMIO_BASE, NV_PVIDEO_SIZE, false,
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
pvideo::reset()
{
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

bool
pvideo::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}

