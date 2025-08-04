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

#define REVISION (USB0_BASE + 0x00)
#define CTRL (USB0_BASE + 0x04)
#define CMD_ST (USB0_BASE + 0x08)
#define RH_DESCRIPTOR_A (USB0_BASE + 0x48)
#define RH_DESCRIPTOR_B (USB0_BASE + 0x4C)
#define RH_ST (USB0_BASE + 0x50)
#define RH_PORT_ST(i) (USB0_BASE + 0x54 + i * 4)


class machine;

struct usb_port {
	uint32_t rh_port_status;
	unsigned idx;
};

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
	enum class usb_state : uint32_t {
		reset = 0,
		resume = 1,
		operational = 2,
		suspend = 3
	};
	bool update_io(bool is_update);
	template<typename T>
	void update_port_status(T &&f);
	void hw_reset();
	void sw_reset();

	machine *const m_machine;
	// registers
	usb_port m_port[4];
	uint32_t m_regs[USB0_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ REVISION, "HcRevision" },
		{ CTRL, "HcControl" },
		{ CMD_ST, "HcCommandStatus" },
		{ RH_DESCRIPTOR_A, "HcRhDescriptorA"},
		{ RH_DESCRIPTOR_B, "HcRhDescriptorB"},
		{ RH_ST, "HcRhStatus"},
		{ RH_PORT_ST(0), "HcRhPortStatus0" },
		{ RH_PORT_ST(1), "HcRhPortStatus1" },
		{ RH_PORT_ST(2), "HcRhPortStatus2" },
		{ RH_PORT_ST(3), "HcRhPortStatus3" },
	};
};
