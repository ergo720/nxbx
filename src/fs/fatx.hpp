// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "files.hpp"
#include "io.hpp"
#include <unordered_map>
#include <vector>

#define FATX_MAX_FILE_LENGTH 42


uint64_t disk_offset_to_partition_offset(uint64_t disk_offset, unsigned &partition_num);

namespace fatx {
	enum cluster_t : uint16_t {
		freed, // not in use
		file, // data_offset is the offset in metadata.bin of the path of the file
		directory, // data_offset is the dirent stream offset in metadata.bin of the directory
		raw, // data_offset is the offset of a raw cluster in metadata.bin
	};

	struct CLUSTER_INFO_ENTRY {
		uint16_t type;
		uint64_t offset;
		uint32_t cluster; // for files only
		char *path; // for files only
		CLUSTER_INFO_ENTRY() : type(cluster_t::freed), offset(0), cluster(0), path(nullptr) {}
		CLUSTER_INFO_ENTRY(uint32_t t, uint64_t o) : type(t), offset(o), cluster(0), path(nullptr) {};
		CLUSTER_INFO_ENTRY(uint32_t t, uint64_t o, uint32_t c, char *p, unsigned partition_num) : type(t), offset(o), cluster(c), path(nullptr)
		{
			if (t == cluster_t::file) {
				assert(IS_HDD_HANDLE(partition_num)); // only device supported right now
				std::string dev_path("Harddisk/Partition");
				std::filesystem::path file_path(dev_path + std::to_string(partition_num - DEV_PARTITION0));
				file_path /= p;
				file_path = to_slash_separator(file_path);
				size_t path_length = file_path.string().length();
				this->path = new char[path_length + 1];
				std::copy_n(file_path.string().c_str(), path_length, this->path);
				this->path[path_length] = '\0';
			}
		}
		~CLUSTER_INFO_ENTRY()
		{
			if (this->path) {
				assert(this->type == cluster_t::file);
				delete[] this->path;
				this->path = nullptr;
			}
		}
	};
	using PCLUSTER_INFO_ENTRY = CLUSTER_INFO_ENTRY *;

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

	class driver {
	public:
		static driver &get(unsigned partition_num)
		{
			static driver m_driver[NUM_OF_DEVS];
			return m_driver[partition_num];
		}
		driver(driver const &) = delete;
		void operator=(driver const &) = delete;
		static bool init(std::filesystem::path hdd_dir);
		static void deinit();
		void flush_dirent_for_file(DIRENT &io_dirent, uint64_t dirent_offset);
		uint64_t get_free_cluster_num();
		io::status_t find_dirent_for_file(std::string_view remaining_path, DIRENT &io_dirent, uint64_t &dirent_offset);
		io::status_t overwrite_dirent_for_file(DIRENT &io_dirent, uint32_t new_size, std::string_view file_path);
		io::status_t create_dirent_for_file(DIRENT &io_dirent, std::string_view file_path);
		io::status_t delete_dirent_for_file(DIRENT &io_dirent);
		io::status_t append_clusters_to_file(DIRENT &io_dirent, int64_t offset, uint32_t size, std::string_view file_path);
		io::status_t read_raw_partition(uint64_t offset, uint32_t size, char *buffer);
		io::status_t write_raw_partition(uint64_t offset, uint32_t size, const char *buffer);
		static io::status_t check_file_access(uint32_t desired_access, uint32_t create_options, uint32_t attributes, bool is_create, uint32_t flags);
		void sync_partition_files();


	private:
		driver() {};
		bool init(std::filesystem::path partition_dir, unsigned partition_num);
		io::status_t is_dirent_stream_empty(uint32_t start_cluster);
		io::status_t extend_dirent_stream(uint32_t cluster, char *cluster_buffer);
		bool format_partition(const char *superblock1, uint32_t offset, uint32_t size);
		bool format_partition(uint32_t cluster_size1);
		bool format_partition();
		void flush_metadata_file();
		template<bool check_is_empty>
		io::status_t scan_dirent_stream(std::string_view remaining_path, DIRENT &io_dirent, uint64_t &dirent_offset, uint32_t start_cluster);
		template<typename T>
		io::status_t extend_cluster_chain(uint32_t start_cluster, uint32_t clusters_to_add, std::string_view file_path);
		template<typename T>
		io::status_t free_allocated_clusters(uint32_t start_cluster, uint32_t clusters_left, std::vector<uint32_t> &found_clusters);
		io::status_t allocate_free_clusters(uint64_t clusters_needed, std::vector<std::pair<uint32_t, uint32_t>> &found_clusters);
		io::status_t update_cluster_table(std::vector<uint32_t> &clusters);
		io::status_t update_cluster_table(uint32_t cluster, uint64_t offset, cluster_t reason);
		io::status_t update_cluster_table(std::vector<std::pair<uint32_t, uint32_t>> &clusters, std::string_view file_path, uint32_t cluster_chain_offset = 0);
		void metadata_set_corrupted_state();
		uint32_t fat_offset_to_cluster(uint64_t offset);
		uint64_t cluster_to_fat_offset(uint32_t cluster);
		CLUSTER_INFO_ENTRY cluster_to_offset(uint32_t cluster);
		bool create_fat();
		bool create_root_dirent();
		bool setup_cluster_info(std::filesystem::path partition_dir);
		bool is_name_valid(const std::string name) const;

		unsigned m_pt_num;
		uint64_t m_metadata_file_size;
		uint64_t m_cluster_table_file_size;
		uint64_t m_cluster_free_num;
		uint64_t m_cluster_size; // in bytes, must be a power of two
		uint64_t m_cluster_shift;
		uint64_t m_cluster_tot_num;
		uint64_t m_metadata_fat_sizes;
		uint64_t m_last_dirent_stream_cluster;
		uint64_t m_last_found_dirent_offset;
		uint64_t m_last_free_dirent_offset;
		uint32_t m_last_allocated_cluster;
		bool m_last_free_dirent_is_on_boundary;
		bool m_metadata_is_corrupted;
		std::fstream m_pt_fs; // fs of partition file
		std::fstream m_ct_fs; // fs of cluster table file
		std::filesystem::path m_ct_path;
		std::unordered_map<uint32_t, CLUSTER_INFO_ENTRY> m_cluster_map;

		// NOTE: these constants are defined in the kernel
		static constexpr uint32_t m_valid_directory_access = 0x11F01FF;
		static constexpr uint32_t m_valid_file_access = 0x11F01FF;
		static constexpr uint32_t m_access_implies_write = 0x11F01B9;
	};
}
