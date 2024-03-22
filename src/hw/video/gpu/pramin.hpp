// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;

class pramin {
public:
	pramin(machine *machine) : m_machine(machine) {}
	bool init();
	void update_io() { update_io(true); }
	template<typename T, bool log = false, bool is_be = false>
	T read(uint32_t addr);
	template<typename T, bool log = false, bool is_be = false>
	void write(uint32_t addr, const T data);

private:
	bool update_io(bool is_update);
	template<bool is_write, typename T>
	auto get_io_func(bool log, bool is_be);
	uint32_t ramin_to_ram_addr(uint32_t ramin_addr);
	machine *const m_machine;
	uint8_t *m_ram;
};
