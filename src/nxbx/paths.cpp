// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#include "paths.hpp"
#include "files.hpp"
#include "io.hpp"
#include "console.hpp"


namespace emu_path
{
	bool setup(const init_info_t &init_info)
	{
		if (!io::setup_paths(init_info)) {
			return false;
		}

#ifdef QT_UI_BUILD
		g_qt_log_path = combine_file_paths(g_nxbx_dir, "qt_log.txt");
#endif
		emu_path::g_krnl_path = init_info.kernel_path;
		emu_path::g_keys_path = init_info.keys_path;

		return true;
	}

	void update_after_reboot(input_t input_type, std::filesystem::path input_path)
	{
		if (!input_path.empty()) {
			if (input_type == input_t::xiso) {
				emu_path::g_xbe_name = "default.xbe";
				emu_path::g_dvd_dir = input_path.remove_filename();
				emu_path::g_xbe_path_xbox = "\\Device\\CdRom0\\" + emu_path::g_xbe_name;
				io::g_dvd_input_type = input_t::xiso;
			}
			else {
				std::filesystem::path local_xbe_path = to_slash_separator(input_path);
				emu_path::g_xbe_name = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(local_xbe_path.filename().string());
				emu_path::g_dvd_dir = local_xbe_path.remove_filename();
				emu_path::g_xbe_path_xbox = "\\Device\\CdRom0\\" + emu_path::g_xbe_name;
				io::g_dvd_input_type = input_t::xbe;
				if (emu_path::g_dvd_dir.string().starts_with(emu_path::g_hdd_dir.string())) {
					// XBE is installed inside a HDD partition, so set the dvd drive to be empty by setting the dvd path to an invalid directory
					size_t partition_num_off = emu_path::g_hdd_dir.string().size() + 9;
					std::string xbox_hdd_dir = "\\Device\\Harddisk0\\Partition" + std::to_string(emu_path::g_dvd_dir.string()[partition_num_off] - '0');
					std::string xbox_remaining_hdd_dir = emu_path::g_dvd_dir.string().substr(partition_num_off + 1);
					std::replace(xbox_remaining_hdd_dir.begin(), xbox_remaining_hdd_dir.end(), '/', '\\'); // convert to xbox path separator
					emu_path::g_xbe_path_xbox = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(xbox_hdd_dir + xbox_remaining_hdd_dir + emu_path::g_xbe_name.c_str());
					emu_path::g_dvd_dir = "";
					g_console->update_tray_state(tray_state::no_media, false);
				}
			}
		}
	}
}
