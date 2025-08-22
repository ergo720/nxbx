// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "settings.hpp"
#include "files.hpp"
#include "logger.hpp"
#include <charconv>


bool
settings::init(const init_info_t &init_info)
{
	m_ini.Reset();
	m_ini.SetMultiKey(true);
	m_console_type = init_info.m_console_type;
	std::filesystem::path curr_dir = init_info.m_nxbx_path;
	curr_dir = curr_dir.remove_filename();
	curr_dir /= "nxbx-" + nxbx::console_to_string(m_console_type) + ".ini";
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
	m_core.log_level = static_cast<log_lv>(m_ini.GetLongValue(m_core_str.name, m_core_str.log_level, std::to_underlying(default_log_lv)));
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
		logger("Mismatching log version, using default log module settings");
	}

	// debugger settings
	m_dbg.version = m_ini.GetLongValue(m_dbg_str.name, m_dbg_str.version, -1);
	// We use the same default values that lib86dbg uses
	m_dbg.width = m_ini.GetLongValue(m_dbg_str.name, m_dbg_str.width, 1280);
	m_dbg.height = m_ini.GetLongValue(m_dbg_str.name, m_dbg_str.height, 720);
	m_dbg.txt_col[0] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.txt_r, 1.0));
	m_dbg.txt_col[1] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.txt_g, 1.0));
	m_dbg.txt_col[2] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.txt_b, 1.0));
	m_dbg.brk_col[0] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.brk_r, 1.0));
	m_dbg.brk_col[1] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.brk_g, 0.0));
	m_dbg.brk_col[2] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.brk_b, 0.0));
	m_dbg.bkg_col[0] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.bkg_r, 0.0));
	m_dbg.bkg_col[1] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.bkg_g, 0.0));
	m_dbg.bkg_col[2] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.bkg_b, 0.0));
	m_dbg.reg_col[0] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.reg_r, 1.0));
	m_dbg.reg_col[1] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.reg_g, 0.0));
	m_dbg.reg_col[2] = static_cast<float>(m_ini.GetDoubleValue(m_dbg_str.name, m_dbg_str.reg_b, 0.0));
	{
		std::string str(m_dbg_str.mem_addr);
		for (unsigned i = 0; i < 4; ++i) {
			str.replace(str.size() - 1, 1, 1, '0' + i);
			m_dbg.mem_addr[i] = get_uint32_value(m_dbg_str.name, str.c_str(), 0);
		}
	}
	m_dbg.mem_active = m_ini.GetLongValue(m_dbg_str.name, m_dbg_str.mem_active, 0);
	std::list<CSimpleIniA::Entry> si_list;
	if (m_ini.GetAllValues(m_dbg_str.name, m_dbg_str.wp, si_list)) {
		for (const auto &elem : si_list) {
			uint32_t value[4]; // addr;idx;size;type
			std::string_view elem_str(elem.pItem);
			for (unsigned i = 0; i < 3; ++i) {
				size_t pos = elem_str.find_first_of(';');
				if (pos == std::string_view::npos) {
					continue; // missing separator between values
				}
				std::string_view value_str = elem_str.substr(0, pos);
				auto ret = std::from_chars(value_str.data(), value_str.data() + value_str.size(), value[i], i == 0 ? 16 : 10);
				if ((ret.ec == std::errc::invalid_argument) || (ret.ec == std::errc::result_out_of_range)) {
					continue; // garbage value
				}
				elem_str = elem_str.substr(pos + 1);
			}
			auto ret = std::from_chars(elem_str.data(), elem_str.data() + elem_str.size(), value[3], 10);
			if ((ret.ec == std::errc::invalid_argument) || (ret.ec == std::errc::result_out_of_range)) {
				continue; // garbage value
			}

			m_dbg.wp_arr[value[1]].addr = value[0];
			m_dbg.wp_arr[value[1]].size = value[2] & 3;
			m_dbg.wp_arr[value[1]].type = value[3] & 3;
		}
	}
	si_list.clear();
	if (m_ini.GetAllValues(m_dbg_str.name, m_dbg_str.bkr, si_list)) {
		for (const auto &elem : si_list) {
			std::string_view addr_str(elem.pItem);
			uint32_t addr;
			auto ret = std::from_chars(addr_str.data(), addr_str.data() + addr_str.size(), addr, 16);
			if ((ret.ec == std::errc::invalid_argument) || (ret.ec == std::errc::result_out_of_range)) {
				continue; // garbage value
			}
			m_dbg.brk_vec.emplace_back(addr);
		}
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

	// debugger settings
	m_ini.Delete(m_dbg_str.name, nullptr, true);
	m_ini.SetLongValue(m_dbg_str.name, m_dbg_str.version, m_dbg.version);
	m_ini.SetLongValue(m_dbg_str.name, m_dbg_str.width, m_dbg.width);
	m_ini.SetLongValue(m_dbg_str.name, m_dbg_str.height, m_dbg.height);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.txt_r, m_dbg.txt_col[0]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.txt_g, m_dbg.txt_col[1]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.txt_b, m_dbg.txt_col[2]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.brk_r, m_dbg.brk_col[0]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.brk_g, m_dbg.brk_col[1]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.brk_b, m_dbg.brk_col[2]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.bkg_r, m_dbg.bkg_col[0]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.bkg_g, m_dbg.bkg_col[1]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.bkg_b, m_dbg.bkg_col[2]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.reg_r, m_dbg.reg_col[0]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.reg_g, m_dbg.reg_col[1]);
	m_ini.SetDoubleValue(m_dbg_str.name, m_dbg_str.reg_b, m_dbg.reg_col[2]);
	{
		std::string str(m_dbg_str.mem_addr);
		for (unsigned i = 0; i < 4; ++i) {
			str.replace(str.size() - 1, 1, 1, '0' + i);
			set_uint32_value(m_dbg_str.name, str.c_str(), m_dbg.mem_addr[i], true);
		}
	}
	m_ini.SetLongValue(m_dbg_str.name, m_dbg_str.mem_active, m_dbg.mem_active);
	for (unsigned i = 0; i < 4; ++i) {
		std::array<char, 50> temp;
		unsigned idx = i;
		uint32_t addr = m_dbg.wp_arr[i].addr;
		uint32_t size = m_dbg.wp_arr[i].size;
		uint32_t type = m_dbg.wp_arr[i].type;
		auto ret = std::to_chars(temp.data(), temp.data() + temp.size(), addr, 16);
		if (ret.ec == std::errc::value_too_large) {
			continue;
		}
		*ret.ptr++ = ';';
		ret = std::to_chars(ret.ptr, temp.data() + temp.size(), idx, 10);
		if (ret.ec == std::errc::value_too_large) {
			continue;
		}
		*ret.ptr++ = ';';
		ret = std::to_chars(ret.ptr, temp.data() + temp.size(), size, 10);
		if (ret.ec == std::errc::value_too_large) {
			continue;
		}
		*ret.ptr++ = ';';
		ret = std::to_chars(ret.ptr, temp.data() + temp.size(), type, 10);
		if (ret.ec == std::errc::value_too_large) {
			continue;
		}
		*ret.ptr = '\0';
		m_ini.SetValue(m_dbg_str.name, m_dbg_str.wp, temp.data());
	}
	for (const auto &addr : m_dbg.brk_vec) {
		std::array<char, 20> temp;
		auto ret = std::to_chars(temp.data(), temp.data() + temp.size(), addr, 16);
		if (ret.ec == std::errc::value_too_large) {
			continue;
		}
		*ret.ptr = '\0';
		m_ini.SetValue(m_dbg_str.name, m_dbg_str.bkr, temp.data());
	}

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
