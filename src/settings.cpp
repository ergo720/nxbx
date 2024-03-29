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
	m_core.version = m_ini.GetLongValue(m_core_str.name, m_core_str.version, m_version);
	m_core.log_version = m_ini.GetLongValue(m_core_str.name, m_core_str.log_version, m_log_version);
	m_core.sys_time_bias = get_int64_value(m_core_str.name, m_core_str.sys_time_bias, 0);
	m_core.log_level = static_cast<log_lv>(m_ini.GetLongValue(m_core_str.name, m_core_str.log_level, static_cast<std::underlying_type_t<log_lv>>(default_log_lv)));
	if (!is_log_lv_in_range(m_core.log_level)) {
		m_core.log_level = default_log_lv;
	}
	if (m_core.log_version == m_log_version) {
		// If the log version matches with the one used by this build of the emulator, then we can safely process the log modules
		// NOTE: don't use m_ini.GetLongValue here, since that will convert 0xFFFFFFFF to 0x7FFFFFFF because of the call to strtol that it uses internally
		m_core.log_modules[0] = get_uint32_value(m_core_str.name, m_core_str.log_modules1, default_log_modules1);
	}
	else {
		// ...otherwise, use default log module settings
		m_core.log_modules[0] = default_log_modules1;
		logger("Mismatching logger version, using default log module settings");
	}
	nxbx::update_logging();
}

bool
settings::save_config_values()
{
	// core settings
	m_ini.SetLongValue(m_core_str.name, m_core_str.version, m_core.version, nullptr, false, true);
	m_ini.SetLongValue(m_core_str.name, m_core_str.log_version, m_log_version, nullptr, false, true);
	set_int64_value(m_core_str.name, m_core_str.sys_time_bias, m_core.sys_time_bias, false);
	m_ini.SetLongValue(m_core_str.name, m_core_str.log_level, (int32_t)m_core.log_level, nullptr, false, true);
	set_uint32_value(m_core_str.name, m_core_str.log_modules1, m_core.log_modules[0], true);

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
settings::set_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_pValue, bool a_bUseHex)
{
	// SimpleIni doesn't implement an api to write keys with 64 bit integer values, so we do it ourselves here
	// TODO: perhaps upstream this?
	
	char szInput[64];
	std::snprintf(szInput, sizeof(szInput), a_bUseHex ? "0x%" PRIx64 : "%" PRId64, a_pValue);
	return m_ini.SetValue(a_pSection, a_pKey, szInput, nullptr, true);
}

uint32_t
settings::get_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault)
{
	const char *pszValue = m_ini.GetValue(a_pSection, a_pKey, nullptr, nullptr);
	if (!pszValue || !*pszValue) {
		return a_nDefault;
	}

	try {
		return static_cast<uint32_t>(std::strtoul(pszValue, nullptr, 0));
	}
	catch (const std::exception &e) {
		logger("Failed to parse value \"%s\" option. The error was: %s", pszValue, e.what());
	}

	return a_nDefault;
}

SI_Error
settings::set_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_pValue, bool a_bUseHex)
{
	char szInput[64];
	std::snprintf(szInput, sizeof(szInput), a_bUseHex ? "0x%" PRIx32 : "%" PRIu32, a_pValue);
	return m_ini.SetValue(a_pSection, a_pKey, szInput, nullptr, true);
}
