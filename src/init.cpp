// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720


#include "init.hpp"
#include "io.hpp"
#include "hw/cpu.hpp"
#include "hw/pic.hpp"
#include "hw/pit.hpp"
#include "hw/cmos.hpp"
#include "../clock.hpp"


static std::vector<hw_reset_f> reset_hw_vec;

void
reset_system()
{
	for (const auto f : reset_hw_vec) {
		f();
	}
}

void
add_reset_func(hw_reset_f reset_f)
{
	reset_hw_vec.push_back(reset_f);
}

void
start_system(std::string kernel, disas_syntax syntax, uint32_t use_dbg, std::string nxbx_path, std::string xbe_path)
{
	if (cpu_init(kernel, syntax, use_dbg)) {
		if (io_init(nxbx_path, xbe_path)) {

			timer_init();

			pic_init();
			pit_init();
			cmos_init();

			cpu_start();
		}
	}

	io_stop();
	cpu_cleanup();
}
