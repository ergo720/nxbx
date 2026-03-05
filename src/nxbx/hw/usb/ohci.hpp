// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2025 ergo720

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <string>

#define USB0_BASE 0xFED00000
#define USB0_SIZE 0x1000
#define REGS_USB0_idx(x) ((x - USB0_BASE) >> 2)
#define REG_USB0(r) (m_regs[REGS_USB0_idx(r)])

#include "ohci_reg_defs.hpp"


class machine;

struct port_status {
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
	uint64_t get_next_update_time(uint64_t now);

private:
	enum class state : uint32_t {
		reset = 0,
		resume = 1,
		operational = 2,
		suspend = 3
	};
	static constexpr uint32_t state_reset = std::to_underlying(state::reset);
	static constexpr uint32_t state_resume = std::to_underlying(state::resume);
	static constexpr uint32_t state_operational = std::to_underlying(state::operational);
	static constexpr uint32_t state_suspend = std::to_underlying(state::suspend);
	static constexpr uint64_t m_usb_freq = 12000000; // 12 MHz
	bool update_io(bool is_update);
	template<typename T>
	void update_port_status(T &&f);
	void update_state(uint32_t value);
	void set_int(uint32_t value);
	void update_int();
	void eof_worker();
	uint32_t calc_frame_left();
	void hw_reset();
	void sw_reset();

	machine *const m_machine;
	bool m_frame_running;
	uint64_t m_sof_time; // time of the sof token, that is, when a new frame starts
	// registers
	port_status m_port[4];
	uint32_t m_regs[USB0_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ REVISION, "REVISION" },
		{ CTRL, "CONTROL" },
		{ CMD_ST, "COMMAND_STATUS" },
		{ INT_ST, "INTERRUPT_STATUS" },
		{ INT_EN, "INTERRUPT_ENABLE"},
		{ INT_DIS, "INTERRUPT_DISABLE"},
		{ HCCA, "HCCA" },
		{ PERIOD_CURR_ED, "PERIOD_CURR_ED" },
		{ CTRL_HEAD_ED, "CONTROL_HEAD_ED" },
		{ CTRL_CURR_ED, "CONTROL_CURRENT_ED" },
		{ BULK_HEAD_ED, "BULK_HEAD_ED" },
		{ BULK_CURR_ED, "BULK_CURRENT_ED" },
		{ DONE_HEAD, "DONE_HEAD" },
		{ FM_INTERVAL, "FRAME_INTERVAL" },
		{ FM_REMAINING, "FRAME_REMAINING" },
		{ FM_NUM, "FRAME_NUM" },
		{ PERIOD_START, "PERIODIC_START" },
		{ LS_THRESHOLD, "LS_THRESHOLD" },
		{ RH_DESCRIPTOR_A, "RHDESCRIPTORA"},
		{ RH_DESCRIPTOR_B, "RHDESCRIPTORB"},
		{ RH_ST, "RHSTATUS"},
		{ RH_PORT_ST(0), "RHPORTSTATUS0" },
		{ RH_PORT_ST(1), "RHPORTSTATUS1" },
		{ RH_PORT_ST(2), "RHPORTSTATUS2" },
		{ RH_PORT_ST(3), "RHPORTSTATUS3" },
	};
};
