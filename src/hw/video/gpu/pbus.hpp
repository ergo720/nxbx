// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


struct pbus_t {
	// Contains the ram type, among other unknown info about the ram modules
	uint32_t fbio_ram;
};

void pbus_init();
