// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pcrtc

#define NV_PCRTC 0x00600000
#define NV_PCRTC_BASE (NV2A_REGISTER_BASE + NV_PCRTC)
#define NV_PCRTC_SIZE 0x1000

#define NV_PCRTC_INTR_0 (NV2A_REGISTER_BASE + 0x00600100)
#define NV_PCRTC_INTR_0_VBLANK_NOT_PENDING 0x00000000
#define NV_PCRTC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00600140)
#define NV_PCRTC_INTR_EN_0_VBLANK_DISABLED 0x00000000
#define NV_PCRTC_UNKNOWN0 (NV2A_REGISTER_BASE + 0x00600804)


template<bool log>
void pcrtc::write(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

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

	case NV_PCRTC_START:
		fb_addr = data & 0x7FFFFFC; // fb is 4 byte aligned
		break;

	case NV_PCRTC_UNKNOWN0:
		unknown[0] = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log>
uint32_t pcrtc::read(uint32_t addr)
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

	case NV_PCRTC_START:
		value = fb_addr;
		break;

	case NV_PCRTC_UNKNOWN0:
		value = unknown[0];
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
pcrtc::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PCRTC_BASE, NV_PCRTC_SIZE, false,
		{
			.fnr32 = log ? cpu_read<pcrtc, uint32_t, &pcrtc::read<true>> : cpu_read<pcrtc, uint32_t, &pcrtc::read<false>>,
			.fnw32 = log ? cpu_write<pcrtc, uint32_t, &pcrtc::write<true>> : cpu_write<pcrtc, uint32_t, &pcrtc::write<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pcrtc::reset()
{
	int_status = NV_PCRTC_INTR_0_VBLANK_NOT_PENDING;
	int_enabled = NV_PCRTC_INTR_EN_0_VBLANK_DISABLED;
	fb_addr = 0;
	for (uint32_t &reg : unknown) {
		reg = 0;
	}
}

bool
pcrtc::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
