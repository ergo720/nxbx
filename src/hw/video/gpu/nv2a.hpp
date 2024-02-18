// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "pbus_pci.hpp"
#include "pfb.hpp"
#include "pmc.hpp"
#include "pcrtc.hpp"
#include "ptimer.hpp"

#define NV2A_CLOCK_FREQ 233333324 // = 233 MHz
#define NV2A_IRQ_NUM 3
#define NV2A_REGISTER_BASE 0xFD000000
#define NV2A_FB_SIZE 0x4000000 // = 64 MiB


struct nv2a_t {
	pmc_t pmc;
	pcrtc_t pcrtc;
	ptimer_t ptimer;
};

inline nv2a_t g_nv2a;

void nv2a_init();
uint64_t nv2a_get_next_update_time(uint64_t now);
