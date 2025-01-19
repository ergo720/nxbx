// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>

#define NV_PUSER 0x00800000
#define NV_PUSER_BASE (NV2A_REGISTER_BASE + NV_PUSER)
#define NV_PUSER_SIZE 0x800000

#define NV_PUSER_DMA_PUT 0x00800040
#define NV_PUSER_DMA_GET 0x00800044
#define NV_PUSER_REF 0x00800048


class machine;

class puser {
public:
	puser(machine *machine) : m_machine(machine) {}
	bool init();
	void update_io() { update_io(true); }
	template<bool log = false>
	uint32_t read32(uint32_t addr);
	template<bool log = false>
	void write32(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool is_be);

	machine *const m_machine;
};
