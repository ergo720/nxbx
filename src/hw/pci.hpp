// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>


// Callback for when a byte in PCI memory is modified. "addr" is the offset, and "ptr" points to the base of the 256-byte block
using pci_conf_write_cb = int(*)(uint8_t *ptr, uint8_t addr, uint8_t data, void *opaque);

class machine;

class pci {
public:
	pci(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	uint8_t read8(uint32_t addr);
	uint16_t read16(uint32_t addr);
	uint32_t read32(uint32_t addr);
	void write8(uint32_t addr, const uint8_t data);
	void write16(uint32_t addr, const uint16_t data);
	void write32(uint32_t addr, const uint32_t data);
	uint8_t read8_logger(uint32_t addr);
	uint16_t read16_logger(uint32_t addr);
	uint32_t read32_logger(uint32_t addr);
	void write8_logger(uint32_t addr, const uint8_t data);
	void write16_logger(uint32_t addr, const uint16_t data);
	void write32_logger(uint32_t addr, const uint32_t data);
	void *create_device(uint32_t bus, uint32_t device, uint32_t function, pci_conf_write_cb cb, void *opaque);
	void copy_default_configuration(void *confptr, void *area, int size);
	void *get_configuration_ptr(uint32_t bus, uint32_t device, uint32_t function);

private:
	bool update_io(bool is_update);

	machine *const m_machine;
	/// ignore: configuration_address_spaces
	uint32_t configuration_address_register;
	// Whether to generate a configuration cycle or not
	int configuration_cycle;
	// Device configuration address space and write callbacks
	std::unordered_map<uint32_t, std::unique_ptr<uint8_t[]>> configuration_address_spaces;
	std::unordered_map<uint32_t, std::pair<pci_conf_write_cb, void *>> configuration_modification;
};
