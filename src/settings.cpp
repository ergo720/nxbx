// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "settings.hpp"
#include "files.hpp"
#include "logger.hpp"


bool
settings::init(const init_info_t &init_info)
{
	m_ini.Reset();
	m_ini.SetMultiKey(true);
	m_type = init_info.m_type;
	std::filesystem::path curr_dir = init_info.m_nxbx_path;
	curr_dir = curr_dir.remove_filename();
	curr_dir /= "nxbx-" + nxbx::console_to_string(m_type) + ".ini";
	m_ini_path = curr_dir.string();
	
	SI_Error err = m_ini.LoadFile(m_ini_path.c_str());
	if (err < 0) {
		// ini file doesn't exist, so create a new one with default values
		if (const auto &opt = create_file(m_ini_path); opt.has_value() == false) {
			return false;
		}

		load_config_values();

		return save_config_values();
	};
	
	load_config_values();
	return true;
}

void
settings::save()
{
	save_config_values();
}

void
settings::load_config_values()
{
	// core settings
	m_core.version = (uint32_t)m_ini.GetLongValue(m_core_str.name, m_core_str.version, m_version);
	m_core.sys_time_bias = get_int64_value(m_core_str.name, m_core_str.sys_time_bias, 0);
	
}

bool
settings::save_config_values()
{
	// core settings
	m_ini.SetLongValue(m_core_str.name, m_core_str.version, m_core.version, nullptr, false, true);
	set_int64_value(m_core_str.name, m_core_str.sys_time_bias, m_core.sys_time_bias);

	return m_ini.SaveFile(m_ini_path.c_str()) >= 0;
}

int64_t
settings::get_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_nDefault)
{
	// SimpleIni doesn't implement an api to read keys with 64 bit integer values, so we do it ourselves here
	// TODO: perhaps upstream this?

	const char *pszValue = m_ini.GetValue(a_pSection, a_pKey, nullptr, nullptr);
	if (!pszValue || !*pszValue) {
		return a_nDefault;
	}

	try {
		return static_cast<int64_t>(std::strtoll(pszValue, nullptr, 0));
	}
	catch (const std::exception &e) {
		logger("Failed to parse value \"%s\" option. The error was: %s", pszValue, e.what());
	}

	return a_nDefault;
}

SI_Error
settings::set_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_pValue)
{
	// SimpleIni doesn't implement an api to write keys with 64 bit integer values, so we do it ourselves here
	// TODO: perhaps upstream this?
	
	std::string value_str = std::to_string(a_pValue);
	return m_ini.SetValue(a_pSection, a_pKey, value_str.c_str(), nullptr, true);
}
