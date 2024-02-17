// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


struct pcrtc_t {
	// Pending vblank interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
	uint32_t int_status;
	// Enable/disable vblank interrupt
	uint32_t int_enabled;
};

void pcrtc_init();
