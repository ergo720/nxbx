// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "smbus.hpp"
#include <atomic>


// NOTE: same state values as used internally by the smc, to avoid conversions
enum class tray_state : uint8_t {
	open = 0x10,
	no_media = 0x40,
	media_detect = 0x60,
};

class machine;

class smc : public smbus_device {
public:
	smc(machine *machine, log_module module_name) : smbus_device(module_name), m_machine(machine) {}
	bool init();
	void deinit() override {}
	void reset();
	uint8_t read_byte(uint8_t command) override;
	void write_byte(uint8_t command, uint8_t value) override;
	void update_tray_state(tray_state state, bool do_int);

private:
	machine *const m_machine;
	static constexpr uint8_t m_version[3] = { 'P', '0', '5' };
	uint8_t m_version_idx;
	uint8_t m_regs[34];
	std::atomic_uint8_t m_tray_state; // atomic because it can be updated by console::update_tray_state
};
