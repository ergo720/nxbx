// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>

#define RAM_SIZE64 0x04000000 // = 64 MiB
#define RAM_SIZE128 0x08000000 // = 128 MiB
#define RAM_SIZE256 0x10000000 // = 256 MiB


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
class lpcbridge;
class hostbridge;
struct BootParams;

class machine
{
public:
	machine();
	~machine();
	bool init(const BootParams &params);
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
	lpcbridge *getLpcBridge();
	hostbridge *getHostBridge();
	cpu_t *get86cpu();
	void raise_irq(uint8_t a);
	void lower_irq(uint8_t a);
	void updateIoLogging();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
