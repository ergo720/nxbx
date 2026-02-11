// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#include "settings.hpp"
#include "files.hpp"
#include "logger.hpp"
#include <charconv>

#if _WIN32
#undef min // avoids conflict with min macro imported by Windows.h from simpleini
#endif


bool
settings::init(const std::string_view ini_path)
{
	m_ini.SetMultiKey(false);
	std::filesystem::path curr_dir = ini_path;
	curr_dir = curr_dir.remove_filename();
	curr_dir /= "nxbx.ini";
	m_path = curr_dir.string();
	
	if (m_ini.LoadFile(m_path.c_str()) < 0) {
		// ini file doesn't exist, so create a new one with default values
		if (const auto &opt = create_file(m_path); opt.has_value() == false) {
			return false;
		}

		reset();
	}
	else {
		// Apply version-specific fixes if necessary
		uint32_t ini_version = get_uint32_value("core", "version", -1);
		if (ini_version == -1) {
			// If it's not found, then reset ini
			reset();
		}
		else if (ini_version < 2) {
			// Versions < 2 have different key names for breakpoints and watchpoints, so discard the debugger section
			m_ini.Delete("debugger", nullptr, true);
			set_uint32_value("debugger", "version", g_dbg_opt.id);
		}

		// Update the version numbers with our values
		set_uint32_value("core", "version", m_ini_version);
		set_uint32_value("core", "log_version", m_log_version);
	}
	
	nxbx::update_logging();
	return true;
}

void
settings::save()
{
	set_long_value("core", "log_level", std::to_underlying(g_log_lv.load()));
	set_uint32_value("core", "log_modules0", g_log_modules[0], true);
	m_ini.SaveFile(m_path.c_str());
}

void
settings::reset()
{
	m_ini.Reset();

	// core settings
	set_uint32_value("core", "version", m_ini_version);
	set_uint32_value("core", "log_version", m_log_version);
	set_int64_value("core", "sys_time_bias", 0);
	set_long_value("core", "log_level", std::to_underlying(g_default_log_lv));
	set_uint32_value("core", "log_modules0", g_default_log_modules0, true);

	// debugger settings
	set_uint32_value("debugger", "version", g_dbg_opt.id);
	set_long_value("debugger", "width", g_dbg_opt.width);
	set_long_value("debugger", "height", g_dbg_opt.height);
	set_float_value("debugger", "text_red", g_dbg_opt.txt_col[0]);
	set_float_value("debugger", "text_green", g_dbg_opt.txt_col[1]);
	set_float_value("debugger", "text_blue", g_dbg_opt.txt_col[2]);
	set_float_value("debugger", "breakpoint_red", g_dbg_opt.brk_col[0]);
	set_float_value("debugger", "breakpoint_green", g_dbg_opt.brk_col[1]);
	set_float_value("debugger", "breakpoint_blue", g_dbg_opt.brk_col[2]);
	set_float_value("debugger", "background_red", g_dbg_opt.bkg_col[0]);
	set_float_value("debugger", "background_green", g_dbg_opt.bkg_col[1]);
	set_float_value("debugger", "background_blue", g_dbg_opt.bkg_col[2]);
	set_float_value("debugger", "register_red", g_dbg_opt.reg_col[0]);
	set_float_value("debugger", "register_green", g_dbg_opt.reg_col[1]);
	set_float_value("debugger", "register_blue", g_dbg_opt.reg_col[2]);
	std::string str("memory_editor_address ");
	for (unsigned i = 0; i < 4; ++i) {
		str.replace(str.size() - 1, 1, 1, '0' + i);
		set_uint32_value("debugger", str.c_str(), g_dbg_opt.mem_editor_addr[i], true);
	}
	set_uint32_value("debugger", "active_memory_editor", g_dbg_opt.mem_active);
	std::vector<std::any> any_vec;
	std::for_each(g_dbg_opt.brk_vec.begin(), g_dbg_opt.brk_vec.end(), [&any_vec](const decltype(g_dbg_opt.brk_vec)::value_type &elem)
		{
			any_vec.emplace_back(elem);
		});
	set_vector_values("debugger", "breakpoint", any_vec);
	any_vec.clear();
	for (unsigned i = 0; i < 4; ++i) {
		any_vec.emplace_back(g_dbg_opt.wp_arr[i]);
	}
	set_vector_values("debugger", "watchpoint", any_vec);
}

long
settings::get_long_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault)
{
	return m_ini.GetLongValue(a_pSection, a_pKey, a_nDefault);
}

uint32_t
settings::get_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault)
{
	const char *pszValue = m_ini.GetValue(a_pSection, a_pKey);
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

int64_t
settings::get_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_nDefault)
{
	// SimpleIni doesn't implement an api to read keys with 64 bit integer values, so we do it ourselves here
	// TODO: perhaps upstream this?

	const char *pszValue = m_ini.GetValue(a_pSection, a_pKey);
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

float
settings::get_float_value(const char *a_pSection, const char *a_pKey, float a_nDefault)
{
	return static_cast<float>(m_ini.GetDoubleValue(a_pSection, a_pKey, a_nDefault));
}

std::vector<std::any>
settings::get_vector_values(const char *a_pSection, const char *a_pKey)
{
	std::vector<std::any> any_vec;
	std::string_view str_k(a_pKey);
	std::string str_wp("watchpoint ");
	std::string str_bk("breakpoint ");

	if (str_k.compare(0, str_k.size(), str_wp, 0, str_wp.size() - 1) == 0) {
		for (unsigned i = 0; i < 4; ++i) {
			str_wp.replace(str_wp.size() - 1, 1, 1, '0' + i);
			if (const char *elem = m_ini.GetValue(a_pSection, str_wp.c_str()); elem) {
				uint32_t value[4]; // addr;idx;size;type
				std::string_view elem_str(elem);
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

				wp_data data;
				data.addr = value[0];
				data.size = value[2] & 3;
				data.type = value[3] & 3;
				any_vec.emplace_back(data);
			}
		}
	}
	else if (str_k.compare(0, str_k.size(), str_bk, 0, str_bk.size() - 1) == 0) {
		int end = m_ini.GetSectionSize("debugger");
		for (int i = 0; i < end; ++i) {
			str_bk.replace(str_bk.size() - 1, 1, 1, '0' + i);
			if (const char *elem = m_ini.GetValue(a_pSection, str_bk.c_str()); elem) {
				std::string_view addr_str(elem);
				addr_t addr;
				auto ret = std::from_chars(addr_str.data() + 2, addr_str.data() + addr_str.size(), addr, 16);
				if ((ret.ec == std::errc::invalid_argument) || (ret.ec == std::errc::result_out_of_range)) {
					continue; // garbage value
				}
				any_vec.emplace_back(addr);
			}
		}
	}
	return any_vec;
}

void
settings::set_long_value(const char *a_pSection, const char *a_pKey, long a_pValue, bool a_bUseHex)
{
	m_ini.SetLongValue(a_pSection, a_pKey, a_pValue, nullptr, a_bUseHex, true);
}

void
settings::set_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_pValue, bool a_bUseHex)
{
	char szInput[64];
	std::snprintf(szInput, sizeof(szInput), a_bUseHex ? "0x%" PRIx32 : "%" PRIu32, a_pValue);
	m_ini.SetValue(a_pSection, a_pKey, szInput, nullptr, true);
}

void
settings::set_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_pValue, bool a_bUseHex)
{
	// SimpleIni doesn't implement an api to write keys with 64 bit integer values, so we do it ourselves here
	// TODO: perhaps upstream this?

	char szInput[64];
	std::snprintf(szInput, sizeof(szInput), a_bUseHex ? "0x%" PRIx64 : "%" PRId64, a_pValue);
	m_ini.SetValue(a_pSection, a_pKey, szInput, nullptr, true);
}

void
settings::set_float_value(const char *a_pSection, const char *a_pKey, float a_pValue)
{
	m_ini.SetDoubleValue(a_pSection, a_pKey, a_pValue, nullptr, true);
}

void
settings::set_vector_values(const char *a_pSection, const char *a_pKey, std::vector<std::any> a_pValue)
{
	std::string_view str_k(a_pKey);
	std::string str_wp("watchpoint ");
	std::string str_bk("breakpoint ");

	if (str_k.compare(0, str_k.size(), str_wp, 0, str_wp.size() - 1) == 0) {
		for (unsigned i = 0; i < std::min(a_pValue.size(), static_cast<decltype(a_pValue)::size_type>(4)); ++i) {
			std::array<char, 50> temp;
			unsigned idx = i;
			wp_data data = std::any_cast<wp_data>(a_pValue[i]);
			uint32_t addr = data.addr;
			uint32_t size = data.size;
			uint32_t type = data.type;
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
			str_wp.replace(str_wp.size() - 1, 1, 1, '0' + i);
			m_ini.SetValue(a_pSection, str_wp.c_str(), temp.data(), nullptr, true);
		}
	}
	else if (str_k.compare(0, str_k.size(), str_bk, 0, str_bk.size() - 1) == 0) {
		for (unsigned i = 0; i < a_pValue.size(); ++i) {
			const addr_t addr = std::any_cast<addr_t>(a_pValue[i]);
			str_bk.replace(str_bk.size() - 1, 1, 1, '0' + i);
			set_uint32_value(a_pSection, str_bk.c_str(), addr, true);
		}
	}
}
