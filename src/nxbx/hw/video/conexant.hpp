// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "smbus_virt.hpp"
#include <memory>


class machine;

class conexant : public smbus_device
{
public:
	conexant();
	~conexant();
	void init(machine *machine, log_module log_module) override;
	void deinit() override;
	void reset();
	void quick_command(bool command) override;
	uint8_t receive_byte() override;
	void send_byte(uint8_t value) override;
	uint8_t read_byte(uint8_t command) override;
	void write_byte(uint8_t command, uint8_t value) override;
	uint16_t read_word(uint8_t command) override;
	void write_word(uint8_t command, uint16_t value) override;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
