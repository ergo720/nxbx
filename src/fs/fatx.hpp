// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "files.hpp"
#include "io.hpp"

#define FATX_MAX_FILE_LENGTH 42


bool create_partition_metadata_file(std::filesystem::path partition_dir, unsigned partition_num);
void flush_metadata_file(std::fstream *fs, unsigned partition_num);
uint64_t disk_offset_to_partition_offset(uint64_t disk_offset, unsigned &partition_num);

namespace fatx {
#pragma pack(1)
	struct DIRENT {
		uint8_t name_length;
		uint8_t attributes;
		uint8_t name[FATX_MAX_FILE_LENGTH];
		uint32_t first_cluster;
		uint32_t size;
		uint32_t creation_time;
		uint32_t last_write_time;
		uint32_t last_access_time;
	};
	using PDIRENT = DIRENT *;
#pragma pack()


	io::status_t find_dirent_for_file(std::string_view remaining_path, std::fstream *fs, unsigned partition_num, DIRENT &io_dirent, uint64_t &dirent_offset);
	io::status_t create_dirent_for_file(DIRENT &io_dirent, std::fstream *fs, unsigned partition_num, std::string_view file_path);
	io::status_t overwrite_dirent_for_file(DIRENT &io_dirent, std::fstream *fs, uint32_t new_size, unsigned partition_num, std::string_view file_path);
	io::status_t delete_dirent_for_file(DIRENT &io_dirent, std::fstream *fs, unsigned partition_num);
	io::status_t is_dirent_stream_empty(std::fstream *fs, unsigned partition_num, uint32_t start_cluster);
	io::status_t append_clusters_to_file(DIRENT &io_dirent, std::fstream *fs, int64_t offset, uint32_t size, unsigned partition_num, std::string_view file_path);
	io::status_t check_file_access(uint32_t desired_access, uint32_t create_options, uint32_t attributes, bool is_create, uint32_t flags);
	io::status_t read_raw_partition(uint64_t offset, uint32_t size, char *buffer, unsigned partition_num, std::fstream *fs);
	io::status_t write_raw_partition(uint64_t offset, uint32_t size, const char *buffer, unsigned partition_num, std::fstream *fs);
	void flush_dirent_for_file(DIRENT &io_dirent, uint64_t dirent_offset, std::fstream *fs, unsigned partition_num);
	uint64_t get_free_cluster_num(unsigned partition_num);
}
