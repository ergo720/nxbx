// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"
#include "adm1032.hpp"
#include "host.hpp"
#include <cinttypes>

#define MODULE_NAME adm1032


/** Private device implementation **/
class adm1032::Impl
{
public:
	uint8_t read_byte(uint8_t command);
};

uint8_t adm1032::Impl::read_byte(uint8_t command)
{
	switch (command)
	{
	case 0:
		return 40; // motherboard temperature

	case 1:
		return 45; // cpu temperature

	default:
		nxbx_fatal("Unhandled read with command 0x%" PRIX8, command);
	}

	return 0;
}

/** Public interface implementation **/
void adm1032::init(machine *machine, log_module log_module)
{
	m_log_module = log_module;
}

void adm1032::deinit()
{
	// empty
}

uint8_t adm1032::read_byte(uint8_t command)
{
	return m_impl->read_byte(command);
}

adm1032::adm1032() : m_impl{std::make_unique<adm1032::Impl>()} {}
adm1032::~adm1032() {}
