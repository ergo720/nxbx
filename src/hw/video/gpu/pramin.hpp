// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;

class pramin {
public:
	pramin(machine *machine) : m_machine(machine) {}
	bool init();
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

private:
	bool update_io(bool is_update);
	uint32_t ramin_to_ram_addr(uint32_t ramin_addr);
	machine *const m_machine;
	uint8_t *m_ram;
};
