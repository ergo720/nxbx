// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "smbus.hpp"


class adm1032 : public smbus_device {
public:
	adm1032(log_module module_name) : smbus_device(module_name) {}
	void deinit() override {}
	void reset() {}
	uint8_t read_byte(uint8_t command) override;
};
