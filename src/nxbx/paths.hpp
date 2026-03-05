// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#pragma once

#include "util.hpp"
#include "host.hpp"
#include <filesystem>


// _name: name of file only
// _path: full name, aka file path + file name
// _dir: file path without the name at the end
// _xbox: path/dir uses xbox naming convention (e.g.: xbox always uses backslashes as the directory separator)

namespace emu_path
{
	inline util::xbox_string g_xbe_name; // name of the XBE that was loaded
	inline util::xbox_string g_xbe_path_xbox; // full path name of the XBE that was loaded, as used by xbox
	inline std::filesystem::path g_nxbx_dir; // nxbx root directory
	inline std::filesystem::path g_hdd_dir; // root directory of the HDD partition folders
	inline std::filesystem::path g_dvd_dir; // root directory of the folder that contains the DVD image that was loaded
	inline std::filesystem::path g_krnl_path; // full path name of nboxkrnl
	inline std::filesystem::path g_keys_path; // full path name of the keys file
#ifdef QT_UI_BUILD
	inline std::filesystem::path g_qt_log_path;
#endif

	bool setup(const init_info_t &init_info);
	void update_after_reboot(input_t input_type, std::filesystem::path input_path);
}
