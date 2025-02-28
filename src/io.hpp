// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "util.hpp"
#include "nxbx.hpp"
#include <filesystem>

// Device number
#define DEV_CDROM      0
#define DEV_UNUSED     1
#define DEV_PARTITION0 2
#define DEV_PARTITION1 3
#define DEV_PARTITION2 4
#define DEV_PARTITION3 5
#define DEV_PARTITION4 6
#define DEV_PARTITION5 7
#define DEV_PARTITION6 8 // non-standard
#define DEV_PARTITION7 9 // non-standard
#define NUM_OF_DEVS    10

// Special internal handles used by the kernel
#define CDROM_HANDLE      DEV_CDROM
#define UNUSED_HANDLE     DEV_UNUSED
#define PARTITION0_HANDLE DEV_PARTITION0
#define PARTITION1_HANDLE DEV_PARTITION1
#define PARTITION2_HANDLE DEV_PARTITION2
#define PARTITION3_HANDLE DEV_PARTITION3
#define PARTITION4_HANDLE DEV_PARTITION4
#define PARTITION5_HANDLE DEV_PARTITION5
#define PARTITION6_HANDLE DEV_PARTITION6 // non-standard
#define PARTITION7_HANDLE DEV_PARTITION7 // non-standard
#define FIRST_FREE_HANDLE NUM_OF_DEVS
#define IS_DEV_HANDLE(handle) (handle < FIRST_FREE_HANDLE)
#define IS_HDD_HANDLE(handle) ((handle >= PARTITION0_HANDLE) && (handle <= PARTITION7_HANDLE))

#define XBOX_NUM_OF_HDD_PARTITIONS 6

#define IO_MAX_FILE_LENGTH 42
#define IO_FILE_READONLY  0x01
#define IO_FILE_DIRECTORY 0x10


struct cpu_t;

namespace io {
	// These definitions are the same used by nboxkrnl to submit I/O request, and should be kept synchronized with those
	enum status_t : int32_t {
		success = 0,
		pending,
		error,
		failed,
		is_a_directory,
		not_a_directory,
		name_not_found,
		path_not_found,
		corrupt,
		full,
		cannot_delete,
		not_empty,
		is_root_dir // never returned to the kernel
	};

	enum flags_t : uint32_t {
		//is_directory = 1 << 3,
		must_be_a_dir = 1 << 4,
		must_not_be_a_dir = 1 << 5,
	};

	inline bool pending_packets = false;
	inline util::xbox_string xbe_name;
	inline util::xbox_string xbe_path;
	inline std::filesystem::path nxbx_path;
	inline std::filesystem::path hdd_path;
	inline std::filesystem::path dvd_path;

	bool init(const init_info_t &init_info, cpu_t *cpu);
	void stop();
	void submit_io_packet(uint32_t addr);
	void flush_pending_packets();
	void query_io_packet(uint32_t addr);
}
