// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "lib86cpu.h"
#include "../util.hpp"
#include "../nxbx.hpp"
#include <string>

#define RAM_SIZE64 0x4000000 // = 64 MiB
#define RAM_SIZE128 0x8000000 // = 128 MiB


class machine;

class cpu {
public:
	cpu(machine *machine) : m_machine(machine), m_lc86cpu(nullptr) {}
	bool init(const init_info_t &init_info);
	void deinit();
	void reset();
	void start();
	void exit();
	void update_io_logging() { update_io(true); }
	uint64_t check_periodic_events(uint64_t now);
	cpu_t *get_lc86cpu() { return m_lc86cpu; }
	uint32_t get_ramsize() { return m_ramsize; }

private:
	bool update_io(bool is_update);
	uint64_t check_periodic_events();

	machine *const m_machine;
	cpu_t *m_lc86cpu;
	uint32_t m_ramsize;
	bool m_is_dbg_present;
};

template<typename D, typename T, auto f, uint32_t base = 0>
T cpu_read(uint32_t addr, void *opaque)
{
	D *device = static_cast<D *>(opaque);
	return (device->*f)(addr - base);
}

template<typename D, typename T, auto f, uint32_t base = 0>
void cpu_write(uint32_t addr, const T value, void *opaque)
{
	D *device = static_cast<D *>(opaque);
	(device->*f)(addr - base, value);
}
