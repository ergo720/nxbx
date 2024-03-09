// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "../nxbx.hpp"
#include "cpu.hpp"
#include "pic.hpp"
#include "pit.hpp"
#include "cmos.hpp"
#include "pci.hpp"
#include "video/vga.hpp"
#include "video/gpu/nv2a.hpp"


template<typename T>
concept is_cpu_t = std::is_same_v<T, cpu_t *>;

class machine {
public:
	machine() : m_cpu(this), m_pit(this), m_pic{ {this, 0, "MASTER PIC"}, {this, 1, "SLAVE PIC"} }, m_pci(this), m_cmos(this), m_nv2a(this),
	m_vga(this) {}
	bool init(const init_info_t &init_info)
	{
		if (!m_cpu.init(init_info)) {
			return false;
		}
		if (!m_pic[0].init()) {
			return false;
		}
		if (!m_pic[1].init()) {
			return false;
		}
		if (!m_pit.init()) {
			return false;
		}
		if (!m_cmos.init()) {
			return false;
		}
		if (!m_pci.init()) {
			return false;
		}
		if (!m_nv2a.init()) {
			return false;
		}
		if (!m_vga.init()) {
			return false;
		}
		return true;
	}
	void deinit()
	{
		m_cpu.deinit();
	}
	void start() { m_cpu.start(); }
	void exit() { m_cpu.exit(); }
	template<typename T, size_t N = 0>
	requires (!is_cpu_t<T>)
	T &get()
	{
		if constexpr (std::is_same_v<T, cpu>) {
			return m_cpu;
		}
		else if constexpr (std::is_same_v<T, pit>) {
			return m_pit;
		}
		else if constexpr (std::is_same_v<T, pic>) {
			if constexpr (N < 2) {
				return m_pic[N];
			}
			else {
				throw std::logic_error("Out of range index when accessing the PIC array");
			}
		}
		else if constexpr (std::is_same_v<T, pci>) {
			return m_pci;
		}
		else if constexpr (std::is_same_v<T, cmos>) {
			return m_cmos;
		}
		else if constexpr (std::is_same_v<T, vga>) {
			return m_vga;
		}
		else if constexpr (std::is_same_v<T, nv2a>) {
			return m_nv2a;
		}
		else if constexpr (std::is_same_v<T, pmc>) {
			return m_nv2a.get_pmc();
		}
		else if constexpr (std::is_same_v<T, pcrtc>) {
			return m_nv2a.get_pcrtc();
		}
		else if constexpr (std::is_same_v<T, pramdac>) {
			return m_nv2a.get_pramdac();
		}
		else if constexpr (std::is_same_v<T, ptimer>) {
			return m_nv2a.get_ptimer();
		}
		else if constexpr (std::is_same_v<T, pfb>) {
			return m_nv2a.get_pfb();
		}
		else if constexpr (std::is_same_v<T, pbus>) {
			return m_nv2a.get_pbus();
		}
		else if constexpr (std::is_same_v<T, pramin>) {
			return m_nv2a.get_pramin();
		}
		else if constexpr (std::is_same_v<T, pfifo>) {
			return m_nv2a.get_pfifo();
		}
		else if constexpr (std::is_same_v<T, pvga>) {
			return m_nv2a.get_pvga();
		}
		else {
			throw std::logic_error("Attempt to access unknown device");
		}
	}
	template<typename T>
	requires is_cpu_t<T>
	T get()
	{
		if constexpr (std::is_same_v<T, cpu_t *>) {
			return m_cpu.get_lc86cpu();
		}
		else {
			throw std::logic_error("Attempt to access unknown device");
		}
	}
	void raise_irq(uint8_t a)
	{
		m_pic[a > 7 ? 1 : 0].raise_irq(a & 7);
	}
	void lower_irq(uint8_t a)
	{
		m_pic[a > 7 ? 1 : 0].lower_irq(a & 7);
	}

private:
	cpu m_cpu;
	pit m_pit;
	pic m_pic[2]; // 0: master, 1: slave
	pci m_pci;
	cmos m_cmos;
	nv2a m_nv2a;
	vga m_vga;
};
