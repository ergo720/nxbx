// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720


#include "init.hpp"
#include "hw/cpu.hpp"
#include "hw/pic.hpp"
#include "hw/pit.hpp"
#include <array>


static std::array<hw_reset_f, 3> reset_hw_arr;

void
reset_system()
{
	for (const auto f : reset_hw_arr) {
		f();
	}
}

void
add_reset_func(unsigned idx, hw_reset_f reset_f)
{
	reset_hw_arr[idx] = reset_f;
}

void
start_system(std::string kernel, disas_syntax syntax, uint32_t use_dbg)
{
	if (cpu_init(kernel, syntax, use_dbg) == false) {
		cpu_cleanup();
		return;
	}

	pic_init();
	pit_init();

	cpu_start();
}
