// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


// Callback for when a byte in PCI memory is modified. "addr" is the offset, and "ptr" points to the base of the 256-byte block
using pci_conf_write_cb = int(*)(uint8_t *ptr, uint8_t addr, uint8_t data);

void pci_init();
void pci_cleanup();
uint8_t pci_read(uint32_t addr, void *opaque);
void pci_write(uint32_t addr, const uint8_t data, void *opaque);
uint16_t pci_read16(uint32_t addr, void *opaque);
void pci_write16(uint32_t addr, const uint16_t data, void *opaque);
uint32_t pci_read32(uint32_t addr, void *opaque);
void pci_write32(uint32_t addr, const uint32_t data, void *opaque);
