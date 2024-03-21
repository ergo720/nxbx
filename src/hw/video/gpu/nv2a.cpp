// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "nv2a.hpp"


bool
nv2a::init()
{
	if (!m_pmc.init()) {
		return false;
	}
	if (!m_pramdac.init()) {
		return false;
	}
	if (!m_pbus.init()) {
		return false;
	}
	if (!m_pfb.init()) {
		return false;
	}
	if (!m_pcrtc.init()) {
		return false;
	}
	if (!m_ptimer.init()) {
		return false;
	}
	if (!m_pramin.init()) {
		return false;
	}
	if (!m_pfifo.init()) {
		return false;
	}
	if (!m_pvga.init()) {
		return false;
	}
	if (!m_pvideo.init()) {
		return false;
	}
	return true;
}

uint64_t
nv2a::get_next_update_time(uint64_t now)
{
	return m_ptimer.get_next_alarm_time(now);
}

void
nv2a::apply_log_settings()
{
	m_pmc.update_io();
	m_pcrtc.update_io();
	m_pramdac.update_io();
	m_ptimer.update_io();
	m_pfb.update_io();
	m_pbus.update_io();
	m_pramin.update_io();
	m_pfifo.update_io();
	m_pvga.update_io();
	m_pvideo.update_io();
}
