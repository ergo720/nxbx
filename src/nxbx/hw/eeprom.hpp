// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "smbus_virt.hpp"
#include <memory>


class machine;

class eeprom : public smbus_device
{
public:
	eeprom();
	~eeprom();
	void init(machine *machine, log_module log_module) override;
	void deinit() override;
	uint8_t read_byte(uint8_t command) override;
	void write_byte(uint8_t command, uint8_t value) override;
	uint16_t read_word(uint8_t command) override;
	void write_word(uint8_t command, uint16_t value) override;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
