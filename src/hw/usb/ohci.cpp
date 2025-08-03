// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2025 ergo720

#include "machine.hpp"

#define MODULE_NAME usb0

#define usb0_log_read() m_machine->log_read<log_module::MODULE_NAME, false>(m_regs_info, addr, value);
#define usb0_log_write() m_machine->log_write<log_module::MODULE_NAME, false>(m_regs_info, addr, value);


template<bool log>
void usb0::write(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		usb0_log_write();
	}

	switch (addr)
	{
	case HC_REVISION:
		// read-only
		break;

	default:
		REG_USB0(addr) = value;
	}
}

template<bool log>
uint32_t usb0::read(uint32_t addr)
{
	uint32_t value = REG_USB0(addr);

	if constexpr (log) {
		usb0_log_read();
	}

	return value;
}

bool
usb0::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), USB0_BASE, USB0_SIZE, false,
		{
			.fnr32 = log ? cpu_read<usb0, uint32_t, &usb0::read<true>> : cpu_read<usb0, uint32_t, &usb0::read<false>>,
			.fnw32 = log ? cpu_write<usb0, uint32_t, &usb0::write<true>> : cpu_write<usb0, uint32_t, &usb0::write<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
usb0::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	REG_USB0(HC_REVISION) = 0x10;
}

bool
usb0::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
