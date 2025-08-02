// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "smbus.hpp"
#include <fstream>
#include <filesystem>


class eeprom : public smbus_device {
public:
	eeprom(log_module module_name) : smbus_device(module_name) {}
	bool init(std::filesystem::path eeprom_dir);
	void deinit() override;
	uint8_t read_byte(uint8_t command) override;
	void write_byte(uint8_t command, uint8_t value) override;
	uint16_t read_word(uint8_t command) override;
	void write_word(uint8_t command, uint16_t value) override;

private:
	std::fstream m_fs;
	uint8_t m_eeprom[256];
};
