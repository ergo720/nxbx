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

	case NV_PCRTC_START:
		value = fb_addr;
		break;

	case NV_PCRTC_UNKNOWN0:
		value = unknown[0];
		break;

	default:
		nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
	}

	return value;
}

uint32_t
pcrtc::read_logger(uint32_t addr)
{
	uint32_t data = read(addr);
	log_io_read();
	return data;
}

void
pcrtc::write_logger(uint32_t addr, const uint32_t data)
{
	log_io_write();
	write(addr, data);
}

bool
pcrtc::update_io(bool is_update)
{
	bool enable = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PCRTC_BASE, NV_PCRTC_SIZE, false,
		{
			.fnr32 = enable ? cpu_read<pcrtc, uint32_t, &pcrtc::read_logger> : cpu_read<pcrtc, uint32_t, &pcrtc::read>,
			.fnw32 = enable ? cpu_write<pcrtc, uint32_t, &pcrtc::write_logger> : cpu_write<pcrtc, uint32_t, &pcrtc::write>
		},
		this, is_update, is_update))) {
		loggerex1(error, "Failed to update mmio region");
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
