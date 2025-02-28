// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <filesystem>
#include <fstream>


namespace xdvdfs {
	struct file_info_t {
		bool exists; // file was found in the xiso image
		bool is_directory; // file is a directory
		uint64_t offset; // offset of the file inside the xiso image
		uint64_t size; // file size
		int64_t timestamp; // file timestamp
	};


	inline uint64_t xiso_offset; // offset to add to reach the game partition
	inline std::string xiso_name;

	bool validate(std::string_view arg_str);
	file_info_t search_file(std::string_view arg_str);
}
