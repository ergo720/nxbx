// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>


// Callback for when a byte in PCI memory is modified. "addr" is the offset, and "ptr" points to the base of the 256-byte block
using pci_conf_write_cb = int(*)(uint8_t *ptr, uint8_t addr, uint8_t value, void *opaque);

class machine;

class pci
{
public:
	pci();
	~pci();
	bool init(machine *machine);
	void reset();
	void updateIoLogging();
	void *createDevice(uint32_t bus, uint32_t device, uint32_t function, pci_conf_write_cb cb, void *opaque);
	void copyDefaultConfiguration(void *confptr, void *area, size_t size);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
