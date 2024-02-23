// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


struct pmc_t {
	uint32_t endianness;
	// Pending interrupts of all engines
	uint32_t int_status;
	// Enable/disable hw/sw interrupts
	uint32_t int_enabled;
	// Enable/disable gpu engines
	uint32_t engine_enabled;
};

void pmc_init();
void pmc_update_irq();
