// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "cpu.hpp"
#include "pic.hpp"
#include "pit.hpp"
#include "cmos.hpp"
#include "pci.hpp"
#include "smbus.hpp"
#include "eeprom.hpp"
#include "smc.hpp"
#include "adm1032.hpp"
#include "usb/ohci.hpp"
#include "video/conexant.hpp"
#include "video/vga.hpp"
#include "video/gpu/nv2a.hpp"


class machine
{
public:
	machine() : m_cpu(this), m_pit(this), m_pic{ {this, 0, "MASTER PIC"}, {this, 1, "SLAVE PIC"} }, m_pci(this), m_cmos(this), m_nv2a(this),
	m_vga(this), m_smbus(this), m_eeprom(log_module::eeprom), m_smc(this, log_module::smc), m_adm1032(log_module::adm1032), m_conexant(log_module::conexant),
	m_usb0(this) {}
	bool init(const boot_params &params)
	{
		if (!m_cpu.init(params)) {
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
		if (!m_eeprom.init()) {
			return false;
		}
		if (!m_smc.init()) {
			return false;
		}
		if (!m_conexant.init()) {
			return false;
		}
		if (!m_usb0.init()) {
			return false;
		}
		return true;
	}
	void deinit()
	{
		m_cpu.deinit();
		m_cmos.deinit();
		m_smbus.deinit();
	}

	void start() { m_cpu.start(); }
	void exit() { m_cpu.exit(); }

	template<size_t N = 0, typename D, typename R, typename... Args_f, typename... Args_p>
	R invoke(R(D::*f)(Args_f...), Args_p... args)
	{
		D *obj;
		if constexpr (std::is_same_v<D, cpu>) {
			obj = &m_cpu;
		}
		else if constexpr (std::is_same_v<D, pit>) {
			obj = &m_pit;
		}
		else if constexpr (std::is_same_v<D, pic>) {
			if constexpr (N < 2) {
				obj = &m_pic[N];
			}
			else {
				throw std::logic_error("Out of range index when accessing the PIC array");
			}
		}
		else if constexpr (std::is_same_v<D, pci>) {
			obj = &m_pci;
		}
		else if constexpr (std::is_same_v<D, cmos>) {
			obj = &m_cmos;
		}
		else if constexpr (std::is_same_v<D, vga>) {
			obj = &m_vga;
		}
		else if constexpr (std::is_same_v<D, smbus>) {
			obj = &m_smbus;
		}
		else if constexpr (std::is_same_v<D, eeprom>) {
			obj = &m_eeprom;
		}
		else if constexpr (std::is_same_v<D, smc>) {
			obj = &m_smc;
		}
		else if constexpr (std::is_same_v<D, adm1032>) {
			obj = &m_adm1032;
		}
		else if constexpr (std::is_same_v<D, conexant>) {
			obj = &m_conexant;
		}
		else if constexpr (std::is_same_v<D, usb0>) {
			obj = &m_usb0;
		}
		else if constexpr (std::is_same_v<D, nv2a>) {
			obj = &m_nv2a;
		}
		else if constexpr (std::is_same_v<D, pmc>) {
			obj = &m_nv2a.get_pmc();
		}
		else if constexpr (std::is_same_v<D, pcrtc>) {
			obj = &m_nv2a.get_pcrtc();
		}
		else if constexpr (std::is_same_v<D, pramdac>) {
			obj = &m_nv2a.get_pramdac();
		}
		else if constexpr (std::is_same_v<D, ptimer>) {
			obj = &m_nv2a.get_ptimer();
		}
		else if constexpr (std::is_same_v<D, pfb>) {
			obj = &m_nv2a.get_pfb();
		}
		else if constexpr (std::is_same_v<D, pbus>) {
			obj = &m_nv2a.get_pbus();
		}
		else if constexpr (std::is_same_v<D, pramin>) {
			obj = &m_nv2a.get_pramin();
		}
		else if constexpr (std::is_same_v<D, pfifo>) {
			obj = &m_nv2a.get_pfifo();
		}
		else if constexpr (std::is_same_v<D, pvga>) {
			obj = &m_nv2a.get_pvga();
		}
		else if constexpr (std::is_same_v<D, pvideo>) {
			obj = &m_nv2a.get_pvideo();
		}
		else if constexpr (std::is_same_v<D, pgraph>) {
			obj = &m_nv2a.get_pgraph();
		}
		else {
			throw std::logic_error("Attempt to access unknown device");
		}

		if constexpr (std::is_same_v<R, void>) {
			std::invoke(f, obj, args...);
		}
		else {
			return std::invoke(f, obj, args...);
		}
	}

	cpu_t *get_cpu()
	{
		return m_cpu.get_lc86cpu();
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
		m_usb0.update_io_logging();
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
	usb0 m_usb0;
};
