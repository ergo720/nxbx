// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PRMVGA 0x000A0000
#define NV_PRMVGA_BASE (NV2A_REGISTER_BASE + NV_PRMVGA)
#define NV_PRMVGA_SIZE 0x20000
#define NV_PRMVIO 0x000C0000
#define NV_PRMVIO_BASE (NV2A_REGISTER_BASE + NV_PRMVIO)
#define NV_PRMVIO_SIZE 0x8000
#define NV_PRMCIO 0x00601000
#define NV_PRMCIO_BASE (NV2A_REGISTER_BASE + NV_PRMCIO)
#define NV_PRMCIO_SIZE 0x1000
#define NV_PRMDIO 0x00681000
#define NV_PRMDIO_BASE (NV2A_REGISTER_BASE + NV_PRMDIO)
#define NV_PRMDIO_SIZE 0x1000


class machine;

class pvga {
public:
	pvga(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false>
	uint8_t io_read8(uint32_t addr);
	template<bool log = false>
	void io_write8(uint32_t addr, const uint8_t data);
	template<bool log = false>
	void io_write16(uint32_t addr, const uint16_t data);
	template<bool log = false>
	uint8_t mem_read8(uint32_t addr);
	template<bool log = false>
	uint16_t mem_read16(uint32_t addr);
	template<bool log = false>
	void mem_write8(uint32_t addr, const uint8_t data);
	template<bool log = false>
	void mem_write16(uint32_t addr, const uint16_t data);

private:
	void prmvga_log_read(uint32_t addr, uint32_t value);
	void prmvga_log_write(uint32_t addr, uint32_t value);
	bool update_io(bool is_update);
	machine *const m_machine;
};
