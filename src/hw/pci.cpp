// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/pci.cpp

#include "machine.hpp"
#include <cstring>

#define MODULE_NAME pci


template<bool log>
void pci::write8(uint32_t addr, const uint8_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	int offset = addr & 3;
	switch (addr & ~3)
	{
	case 0xCF8: // PCI Configuration Address Register
		offset *= 8;

		configuration_address_register &= ~(0xFF << offset);
		configuration_address_register |= data << offset;

		if (configuration_address_register & 0x7F000003) {
			logger_en(info, "Setting reserved bits of configuration address register");
		}
		configuration_address_register &= ~0x7F000003;
		configuration_cycle = configuration_address_register >> 31;
		break;

	case 0xCFC: // PCI Configuration Data Register
		if (configuration_cycle) {
			int bus = (configuration_address_register >> 16) & 0xFF;
			int device = (configuration_address_register >> 11) & 0x1F;
			int function = (configuration_address_register >> 8) & 7;
			int bdf = (bus << 8) | (device << 3) | function;
			int offset = (configuration_address_register & 0xFC) | (addr & 3);

			const auto it = configuration_modification.find(bdf);
			if (it == configuration_modification.end()) {
				return;
			}
			uint8_t *arr = configuration_address_spaces.find(bdf)->second.get(); // can't fail if above succeeded
			if (!it->second.first(arr, offset, data, it->second.second)) {
				arr[offset] = data;
			}
		}
		break;

	default:
		nxbx_fatal("Write to unknown register - 0x%" PRIX32, addr);
	}
}

template<bool log>
uint8_t pci::read8(uint32_t addr)
{
	int offset = addr & 3;
	uint32_t value = -1;
	switch (addr & ~3)
	{
	case 0xCF8: // PCI Status Register
		value = configuration_address_register >> (offset * 8) & 0xFF;
		break;

	case 0xCFC: { // TODO: Type 0 / Type 1 configuration cycles
		if (configuration_cycle) {
			int bus = configuration_address_register >> 16 & 0xFF;
			int device = (configuration_address_register >> 11) & 0x1F;
			int function = (configuration_address_register >> 8) & 7;
			int offset = configuration_address_register & 0xFC;

			const auto it = configuration_address_spaces.find((bus << 8) | (device << 3) | function);
			if (it == configuration_address_spaces.end()) {
				value = -1;
			}
			else {
				value = it->second[offset | (addr & 3)];
			}
			//status_register = ~value & 0x80000000; // ~(uint8_t value) & 0x80000000 == 0x80000000 and ~(-1) & 0x80000000 == 0
		}
		break;
	}

	default:
		nxbx_fatal("Read from unknown register - 0x%" PRIX32, addr);
		return value;
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

// XXX - provide native 16-bit and 32-bit functions instead of just wrapping around the 8-bit versions.
// Although the PCI spec says that all ports are "Dword-sized," the BochS BIOS reads fractions of registers.

template<bool log>
uint16_t pci::read16(uint32_t addr)
{
	uint16_t value = read8(addr);
	value |= ((uint16_t)read8(addr + 1) << 8);

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
uint32_t pci::read32(uint32_t addr)
{
	uint32_t value = read8(addr);
	value |= ((uint32_t)read8(addr + 1) << 8);
	value |= ((uint32_t)read8(addr + 2) << 16);
	value |= ((uint32_t)read8(addr + 3) << 24);

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void pci::write16(uint32_t addr, const uint16_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	write8(addr, data & 0xFF);
	write8(addr + 1, data >> 8 & 0xFF);
}

template<bool log>
void pci::write32(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	write8(addr, data & 0xFF);
	write8(addr + 1, data >> 8 & 0xFF);
	write8(addr + 2, data >> 16 & 0xFF);
	write8(addr + 3, data >> 24 & 0xFF);
}

void *
pci::create_device(uint32_t bus, uint32_t device, uint32_t function, pci_conf_write_cb cb, void *opaque)
{
	if (bus > 1) {
		nxbx_fatal("Unsupported bus id=%" PRIu32, bus);
		return nullptr;
	}
	if (device > 31) {
		nxbx_fatal("Unsupported device id=%" PRIu32, device);
		return nullptr;
	}
	if (function > 7) {
		nxbx_fatal("Unsupported function id=%" PRIu32, function);
		return nullptr;
	}

	int bdf = (bus << 8) | (device << 3) | function;
	configuration_modification.emplace(bdf, std::make_pair(cb, opaque));
	logger_en(info, "Registering device at bus=%" PRIu32 " device=%" PRIu32 " function=%" PRIu32, bus, device, function);

	return (configuration_address_spaces[bdf] = std::make_unique<uint8_t[]>(256)).get();
}

void
pci::copy_default_configuration(void *confptr, void *area, int size)
{
	memcpy(confptr, area, size > 256 ? 256 : size);
}

void *
pci::get_configuration_ptr(uint32_t bus, uint32_t device, uint32_t function)
{
	if (bus > 1) {
		nxbx_fatal("Unsupported bus id=%" PRIu32, bus);
		return nullptr;
	}
	if (device > 31) {
		nxbx_fatal("Unsupported device id=%" PRIu32, device);
		return nullptr;
	}
	if (function > 7) {
		nxbx_fatal("Unsupported function id=%" PRIu32, function);
		return nullptr;
	}

	return (configuration_address_spaces[(bus << 8) | (device << 3) | function]).get();
}

bool
pci::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0xCF8, 8, true,
		{
			.fnr8 = log ? cpu_read<pci, uint8_t, &pci::read8<true>> : cpu_read<pci, uint8_t, &pci::read8<false>>,
			.fnr16 = log ? cpu_read<pci, uint16_t, &pci::read16<true>> : cpu_read<pci, uint16_t, &pci::read16<false>>,
			.fnr32 = log ? cpu_read<pci, uint32_t, &pci::read32<true>> : cpu_read<pci, uint32_t, &pci::read32<false>>,
			.fnw8 = log ? cpu_write<pci, uint8_t, &pci::write8<true>> : cpu_write<pci, uint8_t, &pci::write8<false>>,
			.fnw16 = log ? cpu_write<pci, uint16_t, &pci::write16<true>> : cpu_write<pci, uint16_t, &pci::write16<false>>,
			.fnw32 = log ? cpu_write<pci, uint32_t, &pci::write32<true>> : cpu_write<pci, uint32_t, &pci::write32<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update io ports");
		return false;
	}

	return true;
}

void
pci::reset()
{
	configuration_address_register = 0;
	configuration_cycle = 0;

	configuration_address_spaces.clear();
	configuration_modification.clear();
}

bool
pci::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
