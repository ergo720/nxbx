// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "smbus_virt.hpp"
#include <memory>


class machine;

class adm1032 : public smbus_device
{
public:
	adm1032();
	~adm1032();
	bool init(machine *machine, log_module module_name) override;
	void deinit() override;
	uint8_t read_byte(uint8_t command) override;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
