// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;

class pvideo {
public:
	pvideo(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log = false, bool enabled = true, bool is_be = false>
	uint32_t read(uint32_t addr);
	template<bool log = false, bool enabled = true, bool is_be = false>
	void write(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);
	template<bool is_write>
	auto get_io_func(bool log, bool enabled, bool is_be);

	machine *const m_machine;
	struct {
		// Some unknown debug stuff
		uint32_t debug[11];
	};
};
