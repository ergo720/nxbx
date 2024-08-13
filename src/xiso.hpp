// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <filesystem>
#include <fstream>


namespace xiso {
	struct file_info_t {
		std::fstream fs;
		bool exists;
		bool is_directory;
		uint64_t offset;
		size_t size;
	};

	inline std::filesystem::path dvd_image_path;
	inline size_t image_offset; // offset to add to reach the game partition

	bool validate(std::string_view arg_str);
	file_info_t search_file(std::string_view arg_str);
}
