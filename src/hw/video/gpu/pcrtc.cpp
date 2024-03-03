// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define NV_PCRTC 0x00600000
#define NV_PCRTC_BASE (NV2A_REGISTER_BASE + NV_PCRTC)
#define NV_PCRTC_SIZE 0x1000

#define NV_PCRTC_INTR_0 (NV2A_REGISTER_BASE + 0x00600100)
#define NV_PCRTC_INTR_0_VBLANK_NOT_PENDING 0x00000000
#define NV_PCRTC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00600140)
#define NV_PCRTC_INTR_EN_0_VBLANK_DISABLED 0x00000000


void
pcrtc::write(uint32_t addr, const uint32_t data)
{
	switch (addr)
	{
	case NV_PCRTC_INTR_0:
		int_status &= ~data;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PCRTC_INTR_EN_0:
		int_enabled = data;
		m_machine->get<pmc>().update_irq();
		break;

	default:
		nxbx::fatal("Unhandled %s write at address 0x%" PRIX32 " with value 0x%" PRIX32, get_name(), addr, data);
	}
}

uint32_t
pcrtc::read(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PCRTC_INTR_0:
		value = int_status;
		break;

	case NV_PCRTC_INTR_EN_0:
		value = int_enabled;
		break;

	default:
		nxbx::fatal("Unhandled %s read at address 0x%" PRIX32, get_name(), addr);
	}

	return value;
}

void
pcrtc::reset()
{
	int_status = NV_PCRTC_INTR_0_VBLANK_NOT_PENDING;
	int_enabled = NV_PCRTC_INTR_EN_0_VBLANK_DISABLED;
}

bool
pcrtc::init()
{
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PCRTC_BASE, NV_PCRTC_SIZE, false,
		{
			.fnr32 = cpu_read<pcrtc, uint32_t, &pcrtc::read>,
			.fnw32 = cpu_write<pcrtc, uint32_t, &pcrtc::write>
		},
		this))) {
		logger(log_lv::error, "Failed to initialize %s mmio region", get_name());
		return false;
	}

	reset();
	return true;
}
