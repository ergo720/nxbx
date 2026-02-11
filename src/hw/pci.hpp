// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// Callback for when a byte in PCI memory is modified. "addr" is the offset, and "ptr" points to the base of the 256-byte block
using pci_conf_write_cb = int(*)(uint8_t *ptr, uint8_t addr, uint8_t value, void *opaque);

class machine;

class pci {
public:
	pci(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	uint16_t read16(uint32_t addr);
	template<bool log = false>
	uint32_t read32(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);
	template<bool log = false>
	void write16(uint32_t addr, const uint16_t value);
	template<bool log = false>
	void write32(uint32_t addr, const uint32_t value);
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
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ PCI_CONFIG_ADDRESS, "CONFIGURATION_ADDRESS" },
		{ PCI_CONFIG_DATA, "CONFIGURATION_DATA" },
	};
};
