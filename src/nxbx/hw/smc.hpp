// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "smbus_virt.hpp"
#include <memory>


// NOTE: same state values as used internally by the smc, to avoid conversions
enum class tray_state : uint8_t {
	open = 0x10,
	no_media = 0x40,
	media_detect = 0x60,
};

class machine;

class smc : public smbus_device
{
public:
	smc();
	~smc();
	bool init(machine *machine, log_module module_name) override;
	void deinit() override;
	void reset();
	uint8_t read_byte(uint8_t command) override;
	void write_byte(uint8_t command, uint8_t value) override;
	void update_tray_state(tray_state state, bool do_int);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
