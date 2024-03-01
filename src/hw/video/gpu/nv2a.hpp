// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "pbus.hpp"
#include "pfb.hpp"
#include "pmc.hpp"
#include "pcrtc.hpp"
#include "pramdac.hpp"
#include "ptimer.hpp"

#define NV2A_CLOCK_FREQ 233333324 // = 233 MHz
#define NV2A_CRYSTAL_FREQ 16666666 // = 16 MHz
#define NV2A_IRQ_NUM 3
#define NV2A_REGISTER_BASE 0xFD000000
#define NV2A_FB_SIZE 0x4000000 // = 64 MiB


class nv2a {
public:
	nv2a(machine *machine) : m_pmc(machine), m_pcrtc(machine), m_pramdac(machine), m_ptimer(machine),
		m_pfb(machine), m_pbus(machine) {}
	bool init();
	uint64_t get_next_update_time(uint64_t now);
	pmc &get_pmc() { return m_pmc; }
	pcrtc &get_pcrtc() { return m_pcrtc; }
	pramdac &get_pramdac() { return m_pramdac; }
	ptimer &get_ptimer() { return m_ptimer; }
	pfb &get_pfb() { return m_pfb; }
	pbus &get_pbus() { return m_pbus; }

private:
	pmc m_pmc;
	pcrtc m_pcrtc;
	pramdac m_pramdac;
	ptimer m_ptimer;
	pfb m_pfb;
	pbus m_pbus;
};
