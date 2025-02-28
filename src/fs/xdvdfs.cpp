// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "xdvdfs.hpp"
#include "files.hpp"
#include "util.hpp"
#include "logger.hpp"
#include <utility>
#include <cstring>

#define SECTOR_SIZE 2048
#define ROOT_DIR_SECTOR 32
#define GAME_PARTITION_OFFSET (SECTOR_SIZE * ROOT_DIR_SECTOR * 6192)
#define FILE_DIRECTORY 0x10


namespace xdvdfs {
#pragma pack(1)
	struct volume_desc_t {
		uint8_t magic1[20];
		uint32_t root_dirent_first_sector;
		uint32_t root_dirent_file_size;
		int64_t timestamp;
		uint8_t unused[1992];
		uint8_t magic2[20];
	};

	struct dirent_t {
		uint16_t left_idx;
		uint16_t right_idx;
		uint32_t file_sector;
		uint32_t file_size;
		uint8_t attributes;
		uint8_t file_name_length;
		char file_name[1]; // on xdvdfs, max length is 255 chars
	};
#pragma pack()

	struct file_entry_t {
		uint16_t left_idx; // offset to add to reach the left dirent on this directory level
		uint16_t right_idx; // offset to add to reach the right dirent on this directory level
		uint32_t file_sector; // sector number of the file pointed by the current dirent
		uint32_t file_size; // size of the file pointed by the current dirent
		uint8_t attributes; // attributes of the file pointed by the current dirent
		char file_name[256]; // name of the file pointed by the current dirent
	};


	static_assert(sizeof(volume_desc_t) == 2048);
	static constexpr char magic[] = { 'M', 'I', 'C', 'R', 'O', 'S', 'O', 'F', 'T', '*', 'X', 'B', 'O', 'X', '*', 'M', 'E', 'D', 'I', 'A' };
	static uint32_t root_dirent_first_sector;
	static std::fstream xiso_fs;
	static int64_t xiso_timestamp;


	bool
	validate(std::string_view arg_str)
	{
		if (auto opt = open_file(arg_str)) {
			// XDVDFS: magic is 20 bytes at start of sector 32 and also at offset 0x7EC of the same sector

			char buff[2048];
			const auto validate_image = [&](size_t offset) {
				opt->seekg(SECTOR_SIZE * ROOT_DIR_SECTOR + offset);
				opt->read(buff, 2048);
				volume_desc_t *volume_desc = (volume_desc_t *)buff;
				if (opt->good() &&
					(std::memcmp(volume_desc->magic1, magic, 20) == 0) &&
					(std::memcmp(volume_desc->magic2, magic, 20) == 0) &&
					(volume_desc->root_dirent_first_sector) &&
					(volume_desc->root_dirent_file_size))
				{
					root_dirent_first_sector = volume_desc->root_dirent_first_sector;
					xiso_fs = std::move(*opt);
					xiso_offset = offset;
					xiso_name = std::filesystem::path(arg_str).filename().string();
					return true;
				}
				return false;
				};

			if (validate_image(0)) {
				logger("Detected scrubbed xiso file");
				return true;
			}

			if (validate_image(GAME_PARTITION_OFFSET)) {
				logger("Detected redump xiso file");
				return true;
			}
		}

		xiso_name = "";

		return false;
	}

	bool
	read_dirent(std::fstream *fs, file_entry_t &file_entry, uint64_t sector, uint64_t offset)
	{
		char buff[SECTOR_SIZE];
		fs->seekg(SECTOR_SIZE * sector + xiso_offset + offset);
		fs->read(buff, 255 + sizeof(dirent_t) - 1);
		if (!fs->good()) {
			fs->clear();
			return false;
		}
		
		dirent_t *dirent = (dirent_t *)buff;
		file_entry.left_idx = dirent->left_idx;
		file_entry.right_idx = dirent->right_idx;
		file_entry.file_sector = dirent->file_sector;
		file_entry.file_size = dirent->file_size;
		file_entry.attributes = dirent->attributes;
		std::copy_n(dirent->file_name, dirent->file_name_length, file_entry.file_name);
		file_entry.file_name[dirent->file_name_length] = '\0';

		return true;
	}

	file_info_t
	search_file(std::string_view arg_str)
	{
		if (arg_str.empty()) {
			// special case: open the root directory of the dvd
			return file_info_t
			{
				.exists = true,
				.is_directory = true,
				.offset = xiso_offset,
				.size = 0,
				.timestamp = xiso_timestamp
			};
		}

		uint64_t offset = 0, curr_pos = 0;
		uint32_t curr_sector = root_dirent_first_sector;
		file_entry_t file_entry;
		util::xbox_string_view path_to_parse = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(arg_str);
		uint64_t pos = std::min(path_to_parse.find_first_of(std::filesystem::path::preferred_separator, curr_pos), path_to_parse.length());
		util::xbox_string_view curr_name = path_to_parse.substr(curr_pos, pos - curr_pos);

		while (true) {
			if (read_dirent(&xiso_fs, file_entry, curr_sector, offset)) {
				int ret = curr_name.compare(file_entry.file_name);
				if (ret < 0) {
					uint64_t new_offset = (uint64_t)file_entry.left_idx << 2;
					if ((new_offset == 0) || // bottom of the tree
						(new_offset <= offset)) { // prevent infinite loops
						return file_info_t{ .exists = false };
					}
					offset = new_offset;
				}
				else if (ret > 0) {
					uint64_t new_offset = (uint64_t)file_entry.right_idx << 2;
					if ((new_offset == 0) || // bottom of the tree
						(new_offset <= offset)) { // prevent infinite loops
						return file_info_t{ .exists = false };
					}
					offset = new_offset;
				}
				else {
					curr_pos = pos + 1;
					pos = std::min(path_to_parse.find_first_of(std::filesystem::path::preferred_separator, curr_pos), path_to_parse.length());
					if (pos == path_to_parse.length()) {
						return file_info_t // processed all path -> we found the requested file/directory
						{
							.exists = true,
							.is_directory = (bool)(file_entry.attributes & FILE_DIRECTORY),
							.offset = file_entry.file_sector * SECTOR_SIZE + xiso_offset,
							.size = file_entry.file_size,
							.timestamp = xiso_timestamp
						};
					}
					// Some path still remains -> we can only proceed if the current file is a directory
					if (file_entry.attributes & FILE_DIRECTORY) {
						offset = 0;
						curr_sector = file_entry.file_sector * SECTOR_SIZE + xiso_offset;
						curr_name = path_to_parse.substr(curr_pos, pos - curr_pos);
						continue;
					}
					break;
				}
			}
			break;
		}

		return file_info_t{ .exists = false };
	}
}
