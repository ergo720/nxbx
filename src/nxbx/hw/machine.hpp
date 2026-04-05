// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>


class cpu;
class pit;
class nv2a;
class pic;
class cmos;
class pci;
class vga;
struct cpu_t;
class smbus;
class eeprom;
class smc;
class adm1032;
class conexant;
class usb0;
struct boot_params;

class machine
{
public:
	machine();
	~machine();
	bool init(const boot_params &params);
	void deinit();
	void start();
	void exit();
	cpu *getCpu();
	pit *getPit();
	pic *getPic(uint32_t N);
	pci *getPci();
	cmos *getCmos();
	vga *getVga();
	smbus *getSmbus();
	eeprom *getEeprom();
	smc *getSmc();
	adm1032 *getAdm1032();
	conexant *getVideoEncoder();
	usb0 *getUsb(uint32_t N);
	nv2a *getGpu();
	cpu_t *get86cpu();
	void raise_irq(uint8_t a);
	void lower_irq(uint8_t a);
	void updateIoLogging();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
