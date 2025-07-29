// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "smbus.hpp"


class conexant : public smbus_device {
public:
	conexant(log_module module_name) : smbus_device(module_name) {}
	bool init();
	void deinit() override {}
	void reset();
	std::optional<uint16_t> quick_command(bool command) override;
	std::optional<uint16_t> receive_byte() override;
	std::optional<uint16_t> send_byte(uint8_t value) override;
	std::optional<uint16_t> read_byte(uint8_t command) override;
	std::optional<uint16_t> write_byte(uint8_t command, uint8_t value) override;
	std::optional<uint16_t> read_word(uint8_t command) override;
	std::optional<uint16_t> write_word(uint8_t command, uint16_t value) override;

private:
	uint8_t m_regs[256];
};
