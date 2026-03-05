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
#include "paths.hpp"
#include "settings.hpp"


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

	std::string SetupKernelPath(std::string kernel_path)
	{
		std::filesystem::path local_path = kernel_path;

		if (local_path.empty()) {
			// Check if the ini file knows a valid kernel path already
			local_path = get_settings()->get_string_value("core", "kernel_path", "");
			if (local_path.empty()) {
				// Attempt to find nboxkrnl in the current directory of nxbx
				local_path = combine_file_paths(emu_path::g_nxbx_dir, "nboxkrnl.exe");
				std::error_code ec;
				bool exists = std::filesystem::exists(local_path, ec);
				if (ec || !exists) {
					return "";
				}
			}
			emu_path::g_krnl_path = local_path;
		}

		get_settings()->set_string_value("core", "kernel_path", local_path.string().c_str());
		return local_path.string();
	}
}
