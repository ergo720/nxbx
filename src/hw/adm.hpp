// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "smbus.hpp"


class adm : public smbus_device {
public:
	adm(log_module module_name) : smbus_device(module_name) {}
	void deinit() override {}
	void reset() {}
	std::optional<uint16_t> read_byte(uint8_t command) override;
};
