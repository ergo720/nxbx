// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "pbus.hpp"
#include "pfb.hpp"
#include "pmc.hpp"
#include "pcrtc.hpp"
#include "pramdac.hpp"
#include "ptimer.hpp"
#include "pramin.hpp"
#include "pfifo.hpp"
#include "pvga.hpp"
#include "pvideo.hpp"
#include "cpu.hpp"

#define NV2A_CLOCK_FREQ 233333324 // = 233 MHz
#define NV2A_CRYSTAL_FREQ 16666666 // = 16 MHz
#define NV2A_IRQ_NUM 3
#define NV2A_REGISTER_BASE 0xFD000000
#define NV2A_VRAM_BASE 0xF0000000
#define NV2A_VRAM_SIZE64 0x4000000 // = 64 MiB
#define NV2A_VRAM_SIZE128 0x8000000 // = 128 MiB


class nv2a {
public:
	nv2a(machine *machine) : m_pmc(machine), m_pcrtc(machine), m_pramdac(machine), m_ptimer(machine),
		m_pfb(machine), m_pbus(machine), m_pramin(machine), m_pfifo(machine), m_pvga(machine), m_pvideo(machine) {}
	bool init();
	uint64_t get_next_update_time(uint64_t now);
	pmc &get_pmc() { return m_pmc; }
	pcrtc &get_pcrtc() { return m_pcrtc; }
	pramdac &get_pramdac() { return m_pramdac; }
	ptimer &get_ptimer() { return m_ptimer; }
	pfb &get_pfb() { return m_pfb; }
	pbus &get_pbus() { return m_pbus; }
	pramin &get_pramin() { return m_pramin; }
	pfifo &get_pfifo() { return m_pfifo; }
	pvga &get_pvga() { return m_pvga; }
	pvideo &get_pvideo() { return m_pvideo; }
	void apply_log_settings();

private:
	pmc m_pmc;
	pcrtc m_pcrtc;
	pramdac m_pramdac;
	ptimer m_ptimer;
	pfb m_pfb;
	pbus m_pbus;
	pramin m_pramin;
	pfifo m_pfifo;
	pvga m_pvga;
	pvideo m_pvideo;
};

template<typename D, typename T, auto f, bool is_be = false, uint32_t base = 0>
T nv2a_read(uint32_t addr, void *opaque)
{
	T value = cpu_read<D, T, f, base>(addr, opaque);
	if constexpr (is_be) {
		value = util::byteswap<T>(value);
	}
	return value;
}

template<typename D, typename T, auto f, bool is_be = false, uint32_t base = 0>
void nv2a_write(uint32_t addr, T value, void *opaque)
{
	if constexpr (is_be) {
		value = util::byteswap<T>(value);
	}
	cpu_write<D, T, f, base>(addr, value, opaque);
}
