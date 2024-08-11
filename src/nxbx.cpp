// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "nxbx.hpp"
#include "console.hpp"
#include "settings.hpp"
#include "files.hpp"
#include "xbe.hpp"
#include "xiso.hpp"


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

			if (xiso::validate(arg_str)) {
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
		return settings::get().init(init_info);
	}

	void
	save_settings()
	{
		settings::get().save();
	}

	template<typename T>
	T &get_settings()
	{
		if constexpr (std::is_same_v<T, core_s>) {
			return settings::get().m_core;
		}
		else {
			throw std::logic_error("Attempt to access unknown settings");
		}
	}

	void
	update_logging()
	{
		g_log_lv = settings::get().m_core.log_level;
		g_log_modules[0] = settings::get().m_core.log_modules[0];
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

	template core_s &get_settings();
}
