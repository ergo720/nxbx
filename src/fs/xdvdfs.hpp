// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "io.hpp"
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

	struct file_entry_t {
		uint16_t left_idx; // offset to add to reach the left dirent on this directory level
		uint16_t right_idx; // offset to add to reach the right dirent on this directory level
		uint32_t file_sector; // sector number of the file pointed by the current dirent
		uint32_t file_size; // size of the file pointed by the current dirent
		uint8_t attributes; // attributes of the file pointed by the current dirent
		char file_name[256]; // name of the file pointed by the current dirent
	};

	class driver {
	public:
		static driver &get()
		{
			static driver m_driver;
			return m_driver;
		}
		driver(driver const &) = delete;
		void operator=(driver const &) = delete;
		bool validate(std::string_view arg_str);
		file_info_t search_file(std::string_view arg_str);
		io::status_t read_raw_disc(uint64_t offset, uint32_t size, char *buffer);

		std::fstream m_xiso_fs; // fs of xiso image file
		static uint64_t g_xiso_offset; // offset to add to reach the game partition
		static std::string g_xiso_name;


	private:
		driver() {};
		bool read_dirent(file_entry_t &file_entry, uint64_t sector, uint64_t offset);

		uint32_t m_root_dirent_first_sector;
		int64_t m_xiso_timestamp; // global timestamp of xiso image
	};
}
