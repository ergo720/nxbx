// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "cpu.hpp"


void
nxbx_fatal(const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger(log_lv::error, msg, args);
	va_end(args);
	cpu_exit(g_cpu);
}
