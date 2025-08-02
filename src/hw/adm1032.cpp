// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME adm1032


uint8_t
adm1032::read_byte(uint8_t command)
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
