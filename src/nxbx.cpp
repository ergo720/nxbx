// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "nxbx.hpp"
#include "console.hpp"
#include "settings.hpp"


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
	start()
	{
		console::get().start();
	}

	void
	exit()
	{
		nxbx::save_settings();
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
	fatal(const char *msg, ...)
	{
		std::va_list args;
		va_start(args, msg);
		logger(log_lv::error, msg, args);
		va_end(args);
		console::get().exit();
	}

	template core_s &get_settings();
}
