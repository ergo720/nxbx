// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#include "nxbx.hpp"
#include "console.hpp"
#include "settings.hpp"
#include "files.hpp"
#include "xbe.hpp"
#include "xdvdfs.hpp"


namespace nxbx {
	static const std::string console_xbox_string("xbox");
	static const std::string console_chihiro_string("chihiro");
	static const std::string console_devkit_string("devkit");
	static const std::string console_unknown_string("unknown");

	bool
	init_console(const init_info_t &init_info)
	{
		return console::get().init(init_info);
	}


	bool
	validate_input_file(init_info_t &init_info, std::string_view arg_str)
	{
		if (auto opt = open_file(arg_str)) {
			if (xbe::validate(arg_str)) {
				init_info.m_input_type = input_t::xbe;
				return true;
			}

			if (xdvdfs::driver::get().validate(arg_str)) {
				init_info.m_input_type = input_t::xiso;
				return true;
			}

			logger("Unrecognized input file (must be an XBE or XISO)");
			return false;
		}

		logger(("Failed to open file \"" + std::string(arg_str) + "\"").c_str());
		return false;
	}

	bool
	init_settings(const init_info_t &init_info)
	{
		return get_settings()->init(init_info.m_nxbx_dir);
	}

	void
	save_settings()
	{
		get_settings()->save();
	}

	isettings *get_settings()
	{
		return &settings::get();
	}

	void
	update_logging()
	{
		g_log_lv = static_cast<log_lv>(get_settings()->get_long_value("core", "log_level", std::to_underlying(g_default_log_lv)));
		g_log_modules[0] = get_settings()->get_uint32_value("core", "log_modules0", g_default_log_modules0);
		console::get().apply_log_settings();
	}

	void
	start()
	{
		console::get().start();
	}

	void
	exit()
	{
		save_settings();
	}

	const std::string &
	console_to_string(console_t type)
	{
		switch (type)
		{
		case console_t::xbox:
			return console_xbox_string;

		case console_t::chihiro:
			return console_chihiro_string;

		case console_t::devkit:
			return console_devkit_string;

		default:
			return console_unknown_string;
		}
	}

	void
	fatal(log_module name, const char *msg, ...)
	{
		std::va_list args;
		va_start(args, msg);
		logger<log_lv::highest, false>(name, msg, args);
		va_end(args);
		console::get().exit();
	}
}
