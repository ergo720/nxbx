// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


struct pramdac_t {
	// core, memory and video clocks
	uint32_t nvpll_coeff, mpll_coeff, vpll_coeff;
	// gpu frequency
	uint64_t core_freq;
};

void pramdac_init();
