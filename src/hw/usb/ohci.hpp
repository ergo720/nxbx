// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2025 ergo720

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <string>

#define USB0_BASE 0xFED00000
#define USB0_SIZE 0x1000
#define REGS_USB0_idx(x) ((x - USB0_BASE) >> 2)
#define REG_USB0(r) (m_regs[REGS_USB0_idx(r)])

#define HC_REVISION (USB0_BASE + 0x00)


class machine;

class usb0 {
public:
	usb0(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	template<bool log>
	uint32_t read(uint32_t addr);
	template<bool log>
	void write(uint32_t addr, const uint32_t value);

private:
	bool update_io(bool is_update);

	machine *const m_machine;
	// registers
	uint32_t m_regs[USB0_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ HC_REVISION, "HcRevision" }
	};
};
