// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#ifdef QT_UI_BUILD
#include "qthost.hpp"
#endif
#include "host.hpp"
#include "files.hpp"
#include "xbe.hpp"
#include "xdvdfs.hpp"
#include "console.hpp"


namespace Host
{
	void
	fatal(log_module name, const char *msg, ...)
	{
		std::va_list args;
		va_start(args, msg);
		logger<log_lv::highest, false>(name, msg, args);
		va_end(args);
		g_console->exit();
	}

	std::expected<input_t, std::string>
	validate_input_file(std::string_view path)
	{
		if (auto opt = open_file(path); opt) {
			if (xbe::validate(path)) {
				return input_t::xbe;
			}

			if (xdvdfs::driver::get().validate(path)) {
				return input_t::xiso;
			}

			return std::unexpected("Unrecognized input file \"" + std::string(path) + "\"");
		}

		return std::unexpected("Failed to open file \"" + std::string(path) + "\"");
	}
}
