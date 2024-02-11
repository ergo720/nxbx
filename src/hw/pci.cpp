// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pci.cpp

#include "cpu.hpp"
#include "pci.hpp"
#include "../init.hpp"


struct pci_t {
	/// ignore: configuration_address_spaces
	uint32_t configuration_address_register;

	// Whether to generate a configuration cycle or not
	int configuration_cycle;

	// Device configuration address space.
	// bus:8 - device:5 - function:3; only nv2a uses bus one so 512 elements are enough to cover all xbox devices
	uint8_t *configuration_address_spaces[256 * 2];

	pci_conf_write_cb configuration_modification[256 * 2];
};

static pci_t g_pci;


void
pci_write(uint32_t addr, const uint8_t data, void *opaque)
{
	int offset = addr & 3;
	switch (addr & ~3)
	{
	case 0xCF8: // PCI Configuration Address Register
		offset *= 8;

		g_pci.configuration_address_register &= ~(0xFF << offset);
		g_pci.configuration_address_register |= data << offset;

		if (g_pci.configuration_address_register & 0x7F000003) {
			logger(log_lv::info, "Setting reserved bits of configuration address register");
		}
		g_pci.configuration_address_register &= ~0x7F000003;
		g_pci.configuration_cycle = g_pci.configuration_address_register >> 31;
		break;

	case 0xCFC: // PCI Configuration Data Register
		if (g_pci.configuration_cycle) {
			int bus = g_pci.configuration_address_register >> 16 & 0xFF;
			int device_and_function = g_pci.configuration_address_register >> 8 & 0xFF;
			int offset = (g_pci.configuration_address_register & 0xFC) | (addr & 3);

			if (bus > 1) [[unlikely]] {
				return;
			}

			if (!g_pci.configuration_modification[(bus << 8) | device_and_function]) {
				return;
			}
			uint8_t *arr = g_pci.configuration_address_spaces[(bus << 8) | device_and_function];
			if (!g_pci.configuration_modification[(bus << 8) | device_and_function](arr, offset, data)) {
				arr[offset] = data;
			}
		}
		break;

	default:
		nxbx_fatal("Write to unknown register - %x", addr);
	}
}

uint8_t
pci_read(uint32_t addr, void *opaque)
{
	int offset = addr & 3;
	uint32_t retval = -1;
	switch (addr & ~3)
	{
	case 0xCF8: // PCI Status Register
		return g_pci.configuration_address_register >> (offset * 8) & 0xFF;

	case 0xCFC: { // TODO: Type 0 / Type 1 configuration cycles
		if (g_pci.configuration_cycle) {
			int bus = g_pci.configuration_address_register >> 16 & 0xFF;
			int device_and_function = g_pci.configuration_address_register >> 8 & 0xFF;
			int offset = g_pci.configuration_address_register & 0xFC;

			if (bus > 1) [[unlikely]] {
				return -1;
			}

			uint8_t *ptr = g_pci.configuration_address_spaces[(bus << 8) | device_and_function];
			if (ptr) {
				retval = ptr[offset | (addr & 3)];
			}
			else {
				retval = -1;
			}
			//g_pci.status_register = ~retval & 0x80000000; // ~(uint8_t value) & 0x80000000 == 0x80000000 and ~(-1) & 0x80000000 == 0
		}
		return retval;
	}

	default:
		nxbx_fatal("Read from unknown register - %x", addr);
		return 0xFF;
	}
}

// XXX - provide native 16-bit and 32-bit functions instead of just wrapping around the 8-bit versions.
// Although the PCI spec says that all ports are "Dword-sized," the BochS BIOS reads fractions of registers.

uint16_t
pci_read16(uint32_t addr, void *opaque)
{
	uint16_t result = pci_read(addr, opaque);
	result |= ((uint16_t)pci_read(addr + 1, opaque) << 8);
	return result;
}

uint32_t
pci_read32(uint32_t addr, void *opaque)
{
	uint32_t result = pci_read(addr, opaque);
	result |= ((uint32_t)pci_read(addr + 1, opaque) << 8);
	result |= ((uint32_t)pci_read(addr + 2, opaque) << 16);
	result |= ((uint32_t)pci_read(addr + 3, opaque) << 24);
	return result;
}

void
pci_write16(uint32_t addr, const uint16_t data, void *opaque)
{
	pci_write(addr, data & 0xFF, opaque);
	pci_write(addr + 1, data >> 8 & 0xFF, opaque);
}

void
pci_write32(uint32_t addr, const uint32_t data, void *opaque)
{
	pci_write(addr, data & 0xFF, opaque);
	pci_write(addr + 1, data >> 8 & 0xFF, opaque);
	pci_write(addr + 2, data >> 16 & 0xFF, opaque);
	pci_write(addr + 3, data >> 24 & 0xFF, opaque);
}

void *
pci_create_device(uint32_t bus, uint32_t device, uint32_t function, pci_conf_write_cb cb)
{
	if (bus > 1) {
		nxbx_fatal("Unsupported bus id=%d", bus);
		return nullptr;
	}
	if (device > 31) {
		nxbx_fatal("Unsupported device id=%d", device);
		return nullptr;
	}
	if (function > 7) {
		nxbx_fatal("Unsupported function id=%d", function);
		return nullptr;
	}

	g_pci.configuration_modification[(bus << 8) | (device << 3) | function] = cb;
	logger(log_lv::info, "Registering device at bus=0 device=%d function=%d", device, function);

	return g_pci.configuration_address_spaces[(bus << 8) | (device << 3) | function] = new uint8_t[256]();
}

void
pci_copy_default_configuration(void *confptr, void *area, int size)
{
	memcpy(confptr, area, size > 256 ? 256 : size);
}

void *
pci_get_configuration_ptr(uint32_t bus, uint32_t device, uint32_t function)
{
	if (bus > 1) {
		nxbx_fatal("Unsupported bus id=%d\n", bus);
		return nullptr;
	}
	if (device > 31) {
		nxbx_fatal("Unsupported device id=%d", device);
		return nullptr;
	}
	if (function > 7) {
		nxbx_fatal("Unsupported function id=%d", function);
		return nullptr;
	}

	return g_pci.configuration_address_spaces[(bus << 8) | (device << 3) | function];
}

void
pci_cleanup()
{
	for (auto &conf : g_pci.configuration_address_spaces) {
		if (conf) {
			delete[] conf;
			conf = nullptr;
		}
	}
}

static void
pci_reset()
{
	g_pci.configuration_address_register = 0;
	g_pci.configuration_cycle = 0;

	pci_cleanup();

	for (auto &cb : g_pci.configuration_modification) {
		cb = nullptr;
	}
}

void
pci_init()
{
	io_handlers_t pci_handlers{ .fnr8 = pci_read, .fnr16 = pci_read16, .fnr32 = pci_read32, .fnw8 = pci_write, .fnw16 = pci_write16, .fnw32 = pci_write32 };
	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, 0xCF8, 8, true, pci_handlers, nullptr))) {
		throw nxbx_exp_abort("Failed to initialize pci I/O ports");
	}

	add_reset_func(pci_reset);
	pci_reset();
}
