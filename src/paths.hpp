// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#pragma once

#include "util.hpp"
#include <filesystem>


// _name: name of file only
// _path: full name, aka file path + file name
// _dir: file path without the name at the end
// _xbox: path/dir uses xbox naming convention (e.g.: xbox always uses backslashes as the directory separator)

namespace emu_path {
	inline util::xbox_string g_xbe_name; // name of the XBE that was loaded
	inline util::xbox_string g_xbe_path_xbox; // full name of the XBE that was loaded, as used by xbox
	inline std::filesystem::path g_nxbx_dir; // nxbx root directory
	inline std::filesystem::path g_hdd_dir; // root directory of the HDD partition folders
	inline std::filesystem::path g_dvd_dir; // root directory of the folder that contains the DVD image that was loaded
}
