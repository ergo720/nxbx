// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "logger.hpp"
#include <string>
#include <cinttypes>


static const char *lv_to_str[(std::underlying_type_t<log_lv>)(log_lv::max)] = {
	"DBG:      ",
	"INFO:     ",
	"WARN:     ",
	"ERROR:    ",
	"CRITICAL: "
};


void
logger(log_lv lv, const char *msg, std::va_list vlist)
{
	if ((std::underlying_type_t<log_lv>)(lv) > (std::underlying_type_t<log_lv>)(log_lv::lowest) &&
		(std::underlying_type_t<log_lv>)(lv) < (std::underlying_type_t<log_lv>)(log_lv::max)) [[likely]] {
		std::string str(lv_to_str[static_cast<std::underlying_type_t<log_lv>>(lv)]);
		str += (msg + '\n');
		std::vprintf(str.c_str(), vlist);
		return;
	}

	logger(log_lv::error, "Unknown log level %" PRId32 " specified to %s", lv, __func__);
	logger(msg, vlist);
}

void
logger(log_lv lv, const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger(lv, msg, args);
	va_end(args);
}

void
logger(const char *msg, std::va_list vlist)
{
	std::string str(msg + '\n');
	std::vprintf(str.c_str(), vlist);
}

void
logger(const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger(msg, args);
	va_end(args);
}
