// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pvideo

#define NV_PVIDEO 0x00008000
#define NV_PVIDEO_BASE (NV2A_REGISTER_BASE + NV_PVIDEO)
#define NV_PVIDEO_SIZE 0x1000

#define NV_PVIDEO_DEBUG_0 (NV2A_REGISTER_BASE + 0x00008080)
#define NV_PVIDEO_DEBUG_1 (NV2A_REGISTER_BASE + 0x00008084)
#define NV_PVIDEO_DEBUG_2 (NV2A_REGISTER_BASE + 0x00008088)
#define NV_PVIDEO_DEBUG_3 (NV2A_REGISTER_BASE + 0x0000808C)
#define NV_PVIDEO_DEBUG_4 (NV2A_REGISTER_BASE + 0x00008090)
#define NV_PVIDEO_DEBUG_5 (NV2A_REGISTER_BASE + 0x00008094)
#define NV_PVIDEO_DEBUG_6 (NV2A_REGISTER_BASE + 0x00008098)
#define NV_PVIDEO_DEBUG_7 (NV2A_REGISTER_BASE + 0x0000809C)
#define NV_PVIDEO_DEBUG_8 (NV2A_REGISTER_BASE + 0x000080A0)
#define NV_PVIDEO_DEBUG_9 (NV2A_REGISTER_BASE + 0x000080A4)
#define NV_PVIDEO_DEBUG_10 (NV2A_REGISTER_BASE + 0x000080A8)


template<bool log>
void pvideo::write(uint32_t addr, const uint32_t data)
{
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

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log>
uint32_t pvideo::read(uint32_t addr)
{
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

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

bool
pvideo::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PVIDEO_BASE, NV_PVIDEO_SIZE, false,
		{
			.fnr32 = log ? cpu_read<pvideo, uint32_t, &pvideo::read<true>> : cpu_read<pvideo, uint32_t, &pvideo::read<false>>,
			.fnw32 = log ? cpu_write<pvideo, uint32_t, &pvideo::write<true>> : cpu_write<pvideo, uint32_t, &pvideo::write<false>>
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

