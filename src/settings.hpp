// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "SimpleIni.h"
#include "nxbx.hpp"
#include <string>


class settings {
public:
	static settings &get()
	{
		static settings m_settings;
		return m_settings;
	}
	settings(settings const &) = delete;
	void operator=(settings const &) = delete;
	bool init(const init_info_t &init_info);
	void save();

	core_s m_core;
	dbg_s m_dbg;

private:
	settings() {}
	void load_config_values();
	bool save_config_values();
	int64_t get_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_nDefault = 0);
	SI_Error set_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_pValue, bool a_bUseHex);
	uint32_t get_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault = 0);
	SI_Error set_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_pValue, bool a_bUseHex);

	CSimpleIniA m_ini;
	struct core_str {
		static constexpr const char *name = "core";
		static constexpr const char *version = "version";
		static constexpr const char *log_version = "log_version";
		static constexpr const char *sys_time_bias = "sys_time_bias";
		static constexpr const char *log_level = "log_level";
		static constexpr const char *log_modules1 = "log_modules1";
	} m_core_str;
	struct dbg_str {
		static constexpr const char *name = "debugger";
		static constexpr const char *version = "version";
		static constexpr const char *width = "width";
		static constexpr const char *height = "height";
		static constexpr const char *txt_r = "text_red";
		static constexpr const char *txt_g = "text_green";
		static constexpr const char *txt_b = "text_blue";
		static constexpr const char *brk_r = "breakpoint_red";
		static constexpr const char *brk_g = "breakpoint_green";
		static constexpr const char *brk_b = "breakpoint_blue";
		static constexpr const char *bkg_r = "background_red";
		static constexpr const char *bkg_g = "background_green";
		static constexpr const char *bkg_b = "background_blue";
		static constexpr const char *bkr = "breakpoint_address";
	} m_dbg_str;
	std::string m_ini_path;
	console_t m_console_type;
	static constexpr uint32_t m_version = 1;
	static constexpr uint32_t m_log_version = 2; // add one to this every time the log modules change
};
