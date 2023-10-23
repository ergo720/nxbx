// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdarg>

enum class log_lv {
	lowest = -1,
	debug,
	info,
	warn,
	error,
	highest,
	max,
};


void logger(log_lv lv, const char *msg, std::va_list vlist);
void logger(log_lv lv, const char *msg, ...);
void logger(const char *msg, std::va_list vlist);
void logger(const char *msg, ...);
