// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "../nxbx.hpp"
#include "cpu.hpp"
#include "pic.hpp"
#include "pit.hpp"
#include "cmos.hpp"
#include "pci.hpp"
#include "smbus.hpp"
#include "eeprom.hpp"
#include "smc.hpp"
#include "adm1032.hpp"
#include "video/conexant.hpp"
#include "video/vga.hpp"
#include "video/gpu/nv2a.hpp"


template<typename T>
concept is_cpu_t = std::is_same_v<T, cpu_t *>;

class machine {
public:
	machine() : m_cpu(this), m_pit(this), m_pic{ {this, 0, "MASTER PIC"}, {this, 1, "SLAVE PIC"} }, m_pci(this), m_cmos(this), m_nv2a(this),
	m_vga(this), m_smbus(this), m_eeprom(log_module::eeprom), m_smc(this, log_module::smc), m_adm1032(log_module::adm1032), m_conexant(log_module::conexant) {}
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
		if (!m_smbus.init()) {
			return false;
		}
		if (!m_eeprom.init(init_info.m_nxbx_path)) {
			return false;
		}
		if (!m_smc.init()) {
			return false;
		}
		if (!m_conexant.init()) {
			return false;
		}
		return true;
	}
	void deinit()
	{
		m_cpu.deinit();
		m_cmos.deinit();
		m_nv2a.deinit();
		m_smbus.deinit();
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
		else if constexpr (std::is_same_v<T, smbus>) {
			return m_smbus;
		}
		else if constexpr (std::is_same_v<T, eeprom>) {
			return m_eeprom;
		}
		else if constexpr (std::is_same_v<T, smc>) {
			return m_smc;
		}
		else if constexpr (std::is_same_v<T, adm1032>) {
			return m_adm1032;
		}
		else if constexpr (std::is_same_v<T, conexant>) {
			return m_conexant;
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
		else if constexpr (std::is_same_v<T, pvideo>) {
			return m_nv2a.get_pvideo();
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
		std::unique_lock lock(pic::m_mtx);
		m_pic[a > 7 ? 1 : 0].raise_irq(a & 7);
	}
	void lower_irq(uint8_t a)
	{
		std::unique_lock lock(pic::m_mtx);
		m_pic[a > 7 ? 1 : 0].lower_irq(a & 7);
	}
	void apply_log_settings()
	{
		m_cpu.update_io_logging();
		m_pit.update_io_logging();
		m_pic[0].update_io_logging();
		m_pic[1].update_io_logging();
		m_pci.update_io_logging();
		m_cmos.update_io_logging();
		m_nv2a.apply_log_settings();
		m_smbus.update_io_logging();
		mem_init_region_io(m_cpu.get_lc86cpu(), 0, 0, true, {}, m_cpu.get_lc86cpu(), true, 3); // trigger the update in lib86cpu too
	}

private:
	cpu m_cpu;
	pit m_pit;
	pic m_pic[2]; // 0: master, 1: slave
	pci m_pci;
	cmos m_cmos;
	nv2a m_nv2a;
	vga m_vga;
	smbus m_smbus;
	eeprom m_eeprom;
	smc m_smc;
	adm1032 m_adm1032;
	conexant m_conexant;
};
