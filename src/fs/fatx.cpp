// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "fatx.hpp"
#include <array>
#include <algorithm>
#include <unordered_map>
#include <limits>
#include <assert.h>

#ifdef __GNUC__
// Ignore multichar warning in the fatx signature macro used below
// Requires at least gcc 13 due to a bug in gcc 4.7.0 that cause the pragma to be ignored,
// see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431 for details
#pragma GCC diagnostic ignored "-Wmultichar"
#endif

#define METADATA_VERSION_NUM 0
#define METADATA_FAT_OFFSET (sizeof(USER_DATA_AREA) + sizeof(fatx::SUPERBLOCK))
#define CLUSTER_TABLE_ELEM_SIZE 4096
#define CLUSTER_TABLE_ENTRIES_PER_ELEM (CLUSTER_TABLE_ELEM_SIZE / sizeof(CLUSTER_DATA_ENTRY))
#define CLUSTER_TO_OFFSET(n) ((n / CLUSTER_TABLE_ENTRIES_PER_ELEM) * CLUSTER_TABLE_ELEM_SIZE + (n % CLUSTER_TABLE_ENTRIES_PER_ELEM) * sizeof(CLUSTER_DATA_ENTRY))

#define NUM_OF_HDD_PARTITIONS 6
#define PE_PARTFLAGS_IN_USE	0x80000000

// NOTE1: the sizes below are in sector units, one sector == 512 bytes
// NOTE2: values adjusted to match the sizes reported by the xboxdevwiki
#define XBOX_HDD_SECTOR_SIZE        (uint64_t)512
#define XBOX_CONFIG_AREA_LBA_START  (0x00000000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_CONFIG_AREA_LBA_SIZE   (0x00080000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_SWAPPART1_LBA_START    (0x00080000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_SWAPPART2_LBA_START    (0x2ee80000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_SWAPPART3_LBA_START    (0x5dc80000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_SWAPPART_LBA_SIZE      (0x2ee00000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_SYSPART_LBA_START      (0x8ca80000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_SYSPART_LBA_SIZE       (0x1f400000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_MUSICPART_LBA_START    (0xabe80000 / XBOX_HDD_SECTOR_SIZE)
#define XBOX_MUSICPART_LBA_SIZE     (0x131f00000 / XBOX_HDD_SECTOR_SIZE)

#define FATX_NAME_LENGTH 32
#define FATX_ONLINE_DATA_LENGTH 2048
#define FATX_RESERVED_LENGTH 1968
#define FATX_SIGNATURE 'XTAF'
#define FATX_MAX_NUM_DIRENT 4096
#define IS_FATX16() ((partition_num >= (2 + DEV_PARTITION0)) && (partition_num <= (5 + DEV_PARTITION0)))

#define FATX16_BOUNDARY     (uint16_t)0xFFF0
#define FATX16_CLUSTER_FREE (uint16_t)0x0000
#define FATX16_CLUSTER_ROOT (uint16_t)0xFFF8
#define FATX16_CLUSTER_EOC  (uint16_t)0xFFFF
#define FATX32_CLUSTER_FREE (uint32_t)0x00000000
#define FATX32_CLUSTER_ROOT (uint32_t)0xFFFFFFF8
#define FATX32_CLUSTER_EOC  (uint32_t)0xFFFFFFFF

#define FATX_DIRENT_END1    0x00
#define FATX_DIRENT_DELETED  0xE5
#define FATX_DIRENT_END2    0xFF

#define FATX_FILE_READONLY  0x01
#define FATX_FILE_DIRECTORY 0x10
#define FATX_DELETE_ON_CLOSE 0x00001000


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
			file_path.make_preferred();
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
// Tracks where a cluster is located in the partition metadata.bin file
struct CLUSTER_DATA_ENTRY {
	uint16_t type; // type of cluster. If free, the other fields are zero
	uint16_t size; // for files, it's the size of its path, otherwise it's zero
	uint32_t info; // for files, it's the cluster offset number inside the file, otherwise it's zero
	uint64_t offset; // offset of the dirent stream (for directories), the raw cluster (for raw), or the path (for files) in the metadata.bin file
};
using PCLUSTER_DATA_ENTRY = CLUSTER_DATA_ENTRY *;

// Arbitrary data area used to store info about the metadata file
struct USER_DATA_AREA {
	uint8_t reserved1[4084];
	uint32_t last_cluster_used; // last cluster that was allocated
	uint8_t is_corrupted; // tracks corruption of fatx metadata
	uint8_t reserved2[3];
	uint32_t version; // version number of metadata file
};
using PUSER_DATA_AREA = USER_DATA_AREA *;
#pragma pack()

namespace fatx {
#pragma pack(1)
	struct XBOX_PARTITION_TABLE {
		uint8_t Magic[16];
		int8_t Res0[32];
		struct TABLE_ENTRY {
			uint8_t Name[16];
			uint32_t Flags;
			uint32_t LBAStart;
			uint32_t LBASize;
			uint32_t Reserved;
		} TableEntries[14];
	};

	struct SUPERBLOCK {
		uint32_t Signature;
		uint32_t VolumeID;
		uint32_t ClusterSize;
		uint32_t RootDirCluster;
		uint16_t Name[FATX_NAME_LENGTH];
		uint8_t OnlineData[FATX_ONLINE_DATA_LENGTH];
		uint8_t Unused[FATX_RESERVED_LENGTH];
	};
	using PSUPERBLOCK = SUPERBLOCK *;
#pragma pack()

	static_assert(sizeof(SUPERBLOCK) == 4096);
	static_assert(sizeof(DIRENT) == 64);
	static_assert(sizeof(USER_DATA_AREA) == 4096);
	static_assert(FATX_MAX_FILE_LENGTH == IO_MAX_FILE_LENGTH);
	static_assert(FATX_FILE_READONLY == IO_FILE_READONLY);
	static_assert(FATX_FILE_DIRECTORY == IO_FILE_DIRECTORY);

	/*
	The layout of a partition.bin metadata file (except for partition zero) is as follows (numbers are given as "offset / size"):
	0 / 4096: user data area
	4096 / 4096: fatx superblock
	8192 / variable: fatx FAT
	8192+sizeof(FAT) / sizeof(one cluster): root dirent stream
	8192+sizeof(FAT)+sizeof(one cluster) / 4096+variable: cluster data area
	*/
	
	/*
	Drive Letter  Description  Offset (bytes)  Size (bytes)  Filesystem       Device Object
	N/A           Config Area  0x00000000      0x00080000    Fixed Structure  \Device\Harddisk0\Partition0
	X             Game Cache   0x00080000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition3
	Y             Game Cache   0x2ee80000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition4
	Z             Game Cache   0x5dc80000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition5
	C             System       0x8ca80000      0x1f400000    FATX 	          \Device\Harddisk0\Partition2
	E             Data         0xabe80000      0x131f00000   FATX 	          \Device\Harddisk0\Partition1
	*/
	// Note that this table ignores the non-standard partitions with drive letters F: and G:. Also not that this partition table doesn't really exist on a stock xbox HDD,
	// as it's only created by homebrews that setup non-standard partitions

	static constexpr XBOX_PARTITION_TABLE hdd_partition_table = {
		{ '*', '*', '*', '*', 'P', 'A', 'R', 'T', 'I', 'N', 'F', 'O', '*', '*', '*', '*' },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{
			{ { 'X', 'B', 'O', 'X', ' ', 'D', 'A', 'T', 'A', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, PE_PARTFLAGS_IN_USE, XBOX_MUSICPART_LBA_START, XBOX_MUSICPART_LBA_SIZE, 0 },
			{ { 'X', 'B', 'O', 'X', ' ', 'S', 'H', 'E', 'L', 'L', ' ', ' ', ' ', ' ', ' ', ' ' }, PE_PARTFLAGS_IN_USE, XBOX_SYSPART_LBA_START, XBOX_SYSPART_LBA_SIZE, 0 },
			{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '1' }, PE_PARTFLAGS_IN_USE, XBOX_SWAPPART1_LBA_START, XBOX_SWAPPART_LBA_SIZE, 0 },
			{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '2' }, PE_PARTFLAGS_IN_USE, XBOX_SWAPPART2_LBA_START, XBOX_SWAPPART_LBA_SIZE, 0 },
			{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '3' }, PE_PARTFLAGS_IN_USE, XBOX_SWAPPART3_LBA_START, XBOX_SWAPPART_LBA_SIZE, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
			{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		}
	};

	// These variables are only accessed by the startup thread during initialization, and then by the io thread at runtime. So, they don't need to use locks
	static XBOX_PARTITION_TABLE current_partition_table;
	static uint64_t cluster_size[NUM_OF_DEVS]; // in bytes, must be a power of two
	static uint64_t cluster_shift[NUM_OF_DEVS];
	static uint64_t cluster_tot_num[NUM_OF_DEVS];
	static uint64_t cluster_free_num[NUM_OF_DEVS];
	static uint64_t metadata_file_size[NUM_OF_DEVS];
	static uint64_t cluster_table_file_size[NUM_OF_DEVS];
	static std::fstream cluster_table_fs[NUM_OF_DEVS];
	static std::filesystem::path cluster_table_path[NUM_OF_DEVS];
	static bool metadata_is_corrupted[NUM_OF_DEVS];

	// Only accessed by the io thread
	static uint32_t last_allocated_cluster[NUM_OF_DEVS];
	static uint64_t last_dirent_stream_cluster;
	static uint64_t last_found_dirent_offset;
	static uint64_t last_free_dirent_offset;
	static uint64_t metadata_fat_sizes[NUM_OF_DEVS];
	static bool last_free_dirent_is_on_boundary;
	static std::unordered_map<uint32_t, CLUSTER_INFO_ENTRY> cluster_map[NUM_OF_DEVS];

	// NOTE: these constants are defined in the kernel
	static constexpr auto fatx_valid_directory_access = 0x11F01FF;
	static constexpr auto fatx_valid_file_access = 0x11F01FF;
	static constexpr auto fatx_access_implies_write = 0x11F01B9;


	static bool
	setup_cluster_info(std::fstream *fs, std::filesystem::path partition_dir, unsigned partition_num)
	{
		// Cache the size, in bytes, of a single cluster
		char buffer[4096 * 2];
		fs->seekg(0, fs->beg);
		fs->read(buffer, sizeof(buffer));
		if (!fs->good()) {
			return false;
		}
		PSUPERBLOCK superblock = (PSUPERBLOCK)(buffer + sizeof(USER_DATA_AREA));
		uint64_t cluster_size1 = superblock->ClusterSize * XBOX_HDD_SECTOR_SIZE;
		switch (cluster_size1)
		{
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
		case 32:
		case 64:
		case 128:
			break;
		default:
			return false;
		}

		// cache the last allocated cluster on the partition
		PUSER_DATA_AREA user_area = (PUSER_DATA_AREA)buffer;
		last_allocated_cluster[partition_num] = user_area->last_cluster_used;

		// Cache the total number of clusters of the partition
		std::string partition0_dir = partition_dir.string().substr(0, partition_dir.string().length() - 1);
		partition0_dir += "0.bin";
		uint64_t partition_length, fat_length;
		if (auto opt = open_file(partition0_dir); !opt) {
			partition_length = hdd_partition_table.TableEntries[partition_num - 1 - DEV_PARTITION0].LBASize * XBOX_HDD_SECTOR_SIZE;
		} else {
			XBOX_PARTITION_TABLE::TABLE_ENTRY table_entry[14];
			opt->seekg(offsetof(XBOX_PARTITION_TABLE, TableEntries), opt->beg);
			opt->read((char *)&table_entry, sizeof(table_entry));
			if (!opt->good()) {
				return false;
			} else {
				partition_length = table_entry[partition_num - 1 - DEV_PARTITION0].LBASize * XBOX_HDD_SECTOR_SIZE;
			}
		}

		if (partition_length == 0) {
			return false;
		}

		cluster_size[partition_num] = cluster_size1;
		cluster_shift[partition_num] = std::countr_zero(cluster_size[partition_num]);
		cluster_tot_num[partition_num] = partition_length / cluster_size1;

		// Calculate the FAT size
		metadata_fat_sizes[partition_num] = ((partition_length / cluster_size1) * (IS_FATX16() ? 2 : 4) + 4095) & ~4095;

		// Cache the total number of free clusters of the partition
		auto count_free_clusters = [fs, partition_num]<typename T>()
			{
				uint64_t fat_length = metadata_fat_sizes[partition_num];
				std::unique_ptr<T[]> fat_buffer = std::make_unique_for_overwrite<T[]>(fat_length / sizeof(T));
				fs->seekg(METADATA_FAT_OFFSET, fs->beg);
				fs->read((char *)fat_buffer.get(), fat_length);
				if (!fs->good()) {
					return false;
				}
				std::for_each(fat_buffer.get(), fat_buffer.get() + fat_length, [partition_num](T fat_entry)
					{
						uint32_t found_cluster;
						if constexpr (sizeof(T) == 2) {
							found_cluster = fat_entry < FATX16_BOUNDARY ? fat_entry : (uint32_t)(int16_t)fat_entry;
						} else {
							found_cluster = fat_entry;
						}
						if (found_cluster == FATX32_CLUSTER_FREE) {
							++cluster_free_num[partition_num];
						}
					});

				return true;
			};
		if (IS_FATX16()) {
			if (!count_free_clusters.template operator()<uint16_t>()) {
				return false;
			}
		} else {
			if (!count_free_clusters.template operator()<uint32_t>()) {
				return false;
			}
		}

		return true;
	}

	static bool
	create_root_dirent(std::fstream *fs, unsigned partition_num)
	{
		uint64_t size = cluster_size[partition_num];
		std::unique_ptr<char[]> buffer = std::make_unique_for_overwrite<char[]>(size);
		std::fill_n(buffer.get(), size, FATX_DIRENT_END2);

		fs->seekp(METADATA_FAT_OFFSET + metadata_fat_sizes[partition_num], fs->beg);
		fs->write(buffer.get(), size);
		return fs->good();
	}

	static bool
	create_fat(std::fstream *fs, unsigned partition_num)
	{
		// NOTE: this assumes that the non-standard partitions are bigger than around 1GiB (fatx16/32 size boundary)
		uint64_t fat_length = metadata_fat_sizes[partition_num];

		// NOTE: place fat_buffer on the heap instead of the stack because it can become quite big, depending on the size of the partition. For example, in many
		// homebrews the non-standard partitions can be up to 927GiB. Assuming a cluster size of 64KiB, this will result in a fat of around 58MiB
		std::unique_ptr<char[]> fat_buffer = std::make_unique_for_overwrite<char[]>(fat_length);
		std::fill(fat_buffer.get(), fat_buffer.get() + fat_length, 0); // mark all clusters as free
		if (IS_FATX16()) {
			uint16_t *fatx16_buffer = (uint16_t *)fat_buffer.get();
			fatx16_buffer[0] = FATX16_CLUSTER_ROOT;
			fatx16_buffer[1] = FATX16_CLUSTER_EOC;
		} else {
			uint32_t *fatx32_buffer = (uint32_t *)fat_buffer.get();
			fatx32_buffer[0] = FATX32_CLUSTER_ROOT;
			fatx32_buffer[1] = FATX32_CLUSTER_EOC;
		}

		fs->seekp(METADATA_FAT_OFFSET, fs->beg);
		fs->write(fat_buffer.get(), fat_length);
		if (!fs->good()) {
			return false;
		}

		return true;
	}

	static CLUSTER_INFO_ENTRY
	cluster_to_offset(uint32_t cluster, unsigned partition_num, std::fstream *fs)
	{
		// First of all, attempt to find the cluster from the cluster map. If that fails, use the table in the metadata table file
		auto it = cluster_map[partition_num].find(cluster);
		if (it != cluster_map[partition_num].end()) {
			return it->second;
		} else {
			uint64_t table_offset = CLUSTER_TO_OFFSET(cluster);
			if (table_offset >= cluster_table_file_size[partition_num]) {
				// This happens when accessing a free cluster that was never allocated before. We don't eagerly cache those
				// in the cluster table to keep its file size small, and we also don't cache this entry in the cluster_map to keep
				// it as small as possible and still avoid the file i/o done below
				return CLUSTER_INFO_ENTRY();
			}
			CLUSTER_DATA_ENTRY data_entry;
			cluster_table_fs[partition_num].seekg(table_offset, cluster_table_fs[partition_num].beg);
			cluster_table_fs[partition_num].read((char *)&data_entry, sizeof(CLUSTER_DATA_ENTRY));
			if (!cluster_table_fs[partition_num].good()) {
				nxbx_mod_fatal(io, "Failed to read ClusterTable%u.bin file", partition_num);
				return CLUSTER_INFO_ENTRY();
			}
			if (data_entry.type == cluster_t::freed) {				
				CLUSTER_INFO_ENTRY info_entry;
				cluster_map[partition_num].insert({ cluster, info_entry });
				return info_entry;
			} else if (data_entry.type == cluster_t::file) {
				char path[256] = { 0 }; // fatx paths are limited to 255
				fs->seekg(data_entry.offset, fs->beg);
				fs->read(path, data_entry.size);
				if (!fs->good()) {
					nxbx_mod_fatal(io, "Failed to read Partition%u.bin file", partition_num);
					return CLUSTER_INFO_ENTRY();
				}
				CLUSTER_INFO_ENTRY info_entry(data_entry.type, data_entry.offset, data_entry.info, path, partition_num);
				cluster_map[partition_num].insert({ cluster, info_entry });
				return info_entry;
			} else {
				CLUSTER_INFO_ENTRY info_entry(data_entry.type, data_entry.offset);
				cluster_map[partition_num].insert({ cluster, info_entry });
				return info_entry;
			}
		}
	}

	static uint64_t
	cluster_to_fat_offset(uint32_t cluster, unsigned partition_num)
	{
		// NOTE: this returns a zero-based fat offset. If using it to seek the metadata.bin file, remember to add METADATA_FAT_OFFSET!
		uint32_t fat_entry_size = IS_FATX16() ? 2 : 4;
		return (cluster - 1) * fat_entry_size;
	}

	static uint32_t
	fat_offset_to_cluster(uint64_t offset, unsigned partition_num)
	{
		// The function assumes that the offset is zero based, and not incremented by METADATA_FAT_OFFSET
		uint32_t fat_entry_size = IS_FATX16() ? 2 : 4;
		return offset / fat_entry_size + 1;
	}

	static void
	metadata_set_corrupted_state(unsigned partition_num)
	{
		metadata_is_corrupted[partition_num] = true;
		nxbx_mod_fatal(io, "Partition %u metadata files have become corrupted, they will be recreated on the next launch of nxbx", partition_num);
	}

	io::status_t
	check_file_access(uint32_t desired_access, uint32_t create_options, uint32_t attributes, bool is_create, uint32_t flags)
	{
		if ((flags & io::flags_t::must_be_a_dir) && !(attributes & FATX_FILE_DIRECTORY)) {
			return io::not_a_directory;
		}
		else if ((flags & io::flags_t::must_not_be_a_dir) && (attributes & FATX_FILE_DIRECTORY)) {
			return io::is_a_directory;
		}

		if (attributes & FATX_FILE_DIRECTORY) {
			if (desired_access & ~fatx_valid_directory_access) {
				return io::failed;
			}
		} else {
			if (desired_access & ~fatx_valid_file_access) {
				return io::failed;
			}
		}

		// The read-only check must be done here because the kernel doesn't know the attribute stored in the dirent
		if (attributes & FATX_FILE_READONLY) {
			if (!is_create && (desired_access & ~fatx_access_implies_write)) {
				return io::failed;
			}
			if (create_options & FATX_DELETE_ON_CLOSE) {
				return io::cannot_delete;
			}
		}

		return io::success;
	}

	static io::status_t
	update_cluster_table(std::vector<std::pair<uint32_t, uint32_t>> &clusters, unsigned partition_num, std::string_view file_path, std::fstream *fs, uint32_t cluster_chain_offset = 0)
	{
		// Overload for a cluster chain belonging to a single file

		assert(!clusters.empty());
		assert(IS_HDD_HANDLE(partition_num)); // only device supported right now

		std::sort(clusters.begin(), clusters.end(), [](const auto &left, const auto &right)
			{
				return left.first < right.first;
			});
		uint32_t highest_cluster = clusters.rbegin()->first;
		uint64_t new_file_table_size = ((highest_cluster + 1) * sizeof(CLUSTER_DATA_ENTRY) + 4095) & ~4095; // align new size to element size

		if (new_file_table_size > cluster_table_file_size[partition_num]) {
			std::error_code ec;
			std::filesystem::resize_file(fatx::cluster_table_path[partition_num], new_file_table_size, ec);
			if (ec) {
				metadata_set_corrupted_state(partition_num);
				return io::status_t::error;
			}
			cluster_table_file_size[partition_num] = new_file_table_size;
		}

		// Write the file relative path to metadata.bin
		std::string dev_path("Harddisk/Partition");
		std::filesystem::path path(dev_path + std::to_string(partition_num - DEV_PARTITION0));
		path /= file_path;
		path.make_preferred();
		size_t path_length = path.string().length();
		assert(path_length <= std::numeric_limits<uint16_t>::max());
		fs->seekp(0, fs->end);
		fs->write(path.string().c_str(), path_length);
		if (!fs->good()) {
			fs->clear();
			metadata_set_corrupted_state(partition_num);
			return io::status_t::error;
		}

		CLUSTER_DATA_ENTRY data_entry;
		data_entry.type = cluster_t::file;
		data_entry.size = (uint16_t)path_length;
		data_entry.offset = metadata_file_size[partition_num];

		// Read one table element, and cache as many as possible clusters to it before flushing it back to table bin
		CLUSTER_DATA_ENTRY table_elem[CLUSTER_TABLE_ENTRIES_PER_ELEM];
		uint64_t aligned_elem_offset = CLUSTER_TO_OFFSET(clusters[0].first) & ~4095;
		uint32_t cluster_elem_base = aligned_elem_offset / CLUSTER_TABLE_ENTRIES_PER_ELEM;
		uint32_t cluster_elem_end = cluster_elem_base + CLUSTER_TABLE_ENTRIES_PER_ELEM - 1;

		const auto read_elem = [&](uint32_t cluster)
			{
				aligned_elem_offset = CLUSTER_TO_OFFSET(cluster) & ~4095;
				cluster_elem_base = aligned_elem_offset / CLUSTER_TABLE_ENTRIES_PER_ELEM;
				cluster_elem_end = cluster_elem_base + CLUSTER_TABLE_ENTRIES_PER_ELEM;
				cluster_table_fs[partition_num].seekg(aligned_elem_offset, cluster_table_fs[partition_num].beg);
				cluster_table_fs[partition_num].read((char *)table_elem, CLUSTER_TABLE_ELEM_SIZE);
				if (!cluster_table_fs[partition_num].good()) {
					cluster_table_fs[partition_num].clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
				return io::status_t::success;
			};

		const auto write_elem = [&]()
			{
				cluster_table_fs[partition_num].seekp(aligned_elem_offset, cluster_table_fs[partition_num].beg);
				cluster_table_fs[partition_num].write((const char *)table_elem, CLUSTER_TABLE_ELEM_SIZE);
				if (!cluster_table_fs[partition_num].good()) {
					cluster_table_fs[partition_num].clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
				return io::status_t::success;
			};

		if (read_elem(clusters[0].first) != io::status_t::success) {
			return io::status_t::error;
		}

		for (const auto &[cluster, chain_offset] : clusters) {
			if (!util::in_range(cluster, cluster_elem_base, cluster_elem_end)) {
				if (write_elem() != io::status_t::success) {
					return io::status_t::error;
				}
				if (read_elem(cluster) != io::status_t::success) {
					return io::status_t::error;
				}
			}
			data_entry.info = chain_offset + cluster_chain_offset;
			table_elem[cluster % CLUSTER_TABLE_ENTRIES_PER_ELEM] = data_entry;
		}

		if (write_elem() != io::status_t::success) {
			return io::status_t::error;
		}

		metadata_file_size[partition_num] += path_length;

		return io::status_t::success;
	}

	static io::status_t
	update_cluster_table(uint32_t cluster, unsigned partition_num, uint64_t offset, cluster_t reason)
	{
		// Overload for a cluster belonging to a single dirent stream or to a raw cluster

		assert((reason == cluster_t::directory) || (reason == cluster_t::raw));

		uint64_t new_file_table_size = ((cluster + 1) * sizeof(CLUSTER_DATA_ENTRY) + 4095) & ~4095; // align new size to element size
		if (new_file_table_size > cluster_table_file_size[partition_num]) {
			std::error_code ec;
			std::filesystem::resize_file(fatx::cluster_table_path[partition_num], new_file_table_size, ec);
			if (ec) {
				metadata_set_corrupted_state(partition_num);
				return io::status_t::error;
			}
			cluster_table_file_size[partition_num] = new_file_table_size;
		}

		CLUSTER_DATA_ENTRY data_entry;
		data_entry.type = reason;
		data_entry.size = 0;
		data_entry.info = 0;
		data_entry.offset = offset;

		uint64_t table_offset = CLUSTER_TO_OFFSET(cluster);
		cluster_table_fs[partition_num].seekp(table_offset, cluster_table_fs[partition_num].beg);
		cluster_table_fs[partition_num].write((const char *)&data_entry, sizeof(CLUSTER_DATA_ENTRY));
		if (!cluster_table_fs[partition_num].good()) {
			cluster_table_fs[partition_num].clear();
			metadata_set_corrupted_state(partition_num);
			return io::status_t::error;
		}

		return io::status_t::success;
	}

	static io::status_t
	update_cluster_table(std::vector<uint32_t> &clusters, unsigned partition_num)
	{
		// Overload to free clusters

		assert(!clusters.empty());
		std::sort(clusters.begin(), clusters.end());
		assert(((*clusters.rbegin() + 1) * sizeof(CLUSTER_DATA_ENTRY)) <= cluster_table_file_size[partition_num]);

		CLUSTER_DATA_ENTRY table_elem[CLUSTER_TABLE_ENTRIES_PER_ELEM];
		uint64_t aligned_elem_offset = CLUSTER_TO_OFFSET(clusters[0]) & ~4095;
		uint32_t cluster_elem_base = aligned_elem_offset / CLUSTER_TABLE_ENTRIES_PER_ELEM;
		uint32_t cluster_elem_end = cluster_elem_base + CLUSTER_TABLE_ENTRIES_PER_ELEM - 1;

		const auto read_elem = [&](uint32_t cluster)
			{
				aligned_elem_offset = CLUSTER_TO_OFFSET(cluster) & ~4095;
				cluster_elem_base = aligned_elem_offset / CLUSTER_TABLE_ENTRIES_PER_ELEM;
				cluster_elem_end = cluster_elem_base + CLUSTER_TABLE_ENTRIES_PER_ELEM;
				cluster_table_fs[partition_num].seekg(aligned_elem_offset, cluster_table_fs[partition_num].beg);
				cluster_table_fs[partition_num].read((char *)table_elem, CLUSTER_TABLE_ELEM_SIZE);
				if (!cluster_table_fs[partition_num].good()) {
					cluster_table_fs[partition_num].clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
				return io::status_t::success;
			};

		const auto write_elem = [&]()
			{
				cluster_table_fs[partition_num].seekp(aligned_elem_offset, cluster_table_fs[partition_num].beg);
				cluster_table_fs[partition_num].write((const char *)table_elem, CLUSTER_TABLE_ELEM_SIZE);
				if (!cluster_table_fs[partition_num].good()) {
					cluster_table_fs[partition_num].clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
				return io::status_t::success;
			};

		if (read_elem(clusters[0]) != io::status_t::success) {
			return io::status_t::error;
		}

		for (const auto &cluster : clusters) {
			if (!util::in_range(cluster, cluster_elem_base, cluster_elem_end)) {
				if (write_elem() != io::status_t::success) {
					return io::status_t::error;
				}
				if (read_elem(cluster) != io::status_t::success) {
					return io::status_t::error;
				}
			}
			table_elem[cluster % CLUSTER_TABLE_ENTRIES_PER_ELEM] = CLUSTER_DATA_ENTRY(cluster_t::freed, 0, 0, 0);
			cluster_map[partition_num].erase(cluster);
		}

		if (write_elem() != io::status_t::success) {
			return io::status_t::error;
		}

		return io::status_t::success;
	}

	static io::status_t
	allocate_free_clusters(uint64_t clusters_needed, unsigned partition_num, std::fstream *fs, std::vector<std::pair<uint32_t, uint32_t>> &found_clusters)
	{
		assert(cluster_free_num[partition_num] >= clusters_needed); // caller should have checked that there are enough clusters available left

		bool first_found_cluster = false;
		constexpr uint64_t size_of_buffer = 4096;
		char fat_buffer[size_of_buffer];
		uint64_t fat_length = metadata_fat_sizes[partition_num];
		uint64_t fat_offset = cluster_to_fat_offset(last_allocated_cluster[partition_num], partition_num), ori_fat_offset = fat_offset;
		uint32_t free_clusters = 0;

		const auto find_free_clusters = [&]<typename T>(uint64_t bytes_left)
			{
				uint64_t prev_buff_offset = 0, curr_buff_offset = 0;

				while (bytes_left > 0) {
					uint32_t found_cluster;
					T fat_entry = *(T *)(fat_buffer + curr_buff_offset);
					if constexpr (sizeof(T) == 2) {
						found_cluster = fat_entry < FATX16_BOUNDARY ? fat_entry : (uint32_t)(int16_t)fat_entry;
					} else {
						found_cluster = fat_entry;
					}
					if (found_cluster == FATX32_CLUSTER_FREE) {
						found_cluster = fat_offset_to_cluster(fat_offset + curr_buff_offset, partition_num);
						found_clusters.emplace_back(found_cluster, free_clusters);
						++free_clusters;
						if (first_found_cluster == false) {
							first_found_cluster = true;
							if (free_clusters == clusters_needed) { // happens when the file requires only one cluster
								*(T *)(fat_buffer + curr_buff_offset) = (T)FATX32_CLUSTER_EOC;
								last_allocated_cluster[partition_num] = found_cluster;
								return true;
							}
							prev_buff_offset = curr_buff_offset;
							bytes_left -= sizeof(T);
							curr_buff_offset += sizeof(T);
							continue; // for the first file cluster, there's no preceding fat entry to chain
						}
						*(T *)(fat_buffer + prev_buff_offset) = (T)found_cluster; // chain the previous entry with the current cluster
						prev_buff_offset = curr_buff_offset;
						if (free_clusters == clusters_needed) {
							*(T *)(fat_buffer + curr_buff_offset) = (T)FATX32_CLUSTER_EOC;
							last_allocated_cluster[partition_num] = found_cluster;
							return true;
						}
					}
					bytes_left -= sizeof(T);
					curr_buff_offset += sizeof(T);
				}

				return false;
			};

		uint64_t fat_bytes_to_end = fat_length - fat_offset, tot_bytes_read = 0;
		retry:
		while (fat_bytes_to_end > 0) {
			uint64_t bytes_to_access = std::min(fat_bytes_to_end, size_of_buffer);
			fs->seekg(fat_offset + METADATA_FAT_OFFSET, fs->beg);
			fs->read(fat_buffer, bytes_to_access);
			if (!fs->good()) {
				fs->clear();
				metadata_set_corrupted_state(partition_num);
				return io::status_t::error;
			}

			bool has_found_enough_clusters;
			if (IS_FATX16()) {
				has_found_enough_clusters = find_free_clusters.template operator()<uint16_t>(bytes_to_access);
			} else {
				has_found_enough_clusters = find_free_clusters.template operator()<uint32_t>(bytes_to_access);
			}

			fs->seekp(fat_offset + METADATA_FAT_OFFSET, fs->beg);
			fs->write(fat_buffer, bytes_to_access);
			if (!fs->good()) {
				fs->clear();
				metadata_set_corrupted_state(partition_num);
				return io::status_t::error;
			}
			if (has_found_enough_clusters) {
				return io::status_t::success;
			}

			fat_bytes_to_end -= bytes_to_access;
			fat_offset += bytes_to_access;
			tot_bytes_read += bytes_to_access;
		}

		if (tot_bytes_read == fat_length) [[unlikely]] {
			assert(0); // unreachable if the assert at the beginning holds
			return io::status_t::error;
		}

		// If we reach here, scan the FAT from the beginning up to last_allocated_cluster_num, since there might be some freed clusters there
		fat_bytes_to_end = ori_fat_offset;
		fat_offset = 0;
		goto retry;
	};

	template<typename T>
	static io::status_t
	free_allocated_clusters(uint32_t start_cluster, uint32_t clusters_left, unsigned partition_num, std::fstream *fs, std::vector<uint32_t> &found_clusters)
	{
		// Move in the chain until we find the position of the new eoc, then free all the remaining chained clusters until we find the old eoc
		// NOTE: If the new size is zero, then there's no new eoc, and all clusters become free

		constexpr uint32_t fat_buffer_size = 4096 / sizeof(T);
		T fat_buffer[fat_buffer_size];
		uint32_t num_of_freed_clusters = 0;
		uint64_t fat_offset = cluster_to_fat_offset(start_cluster, partition_num);
		uint32_t found_cluster = start_cluster, buffer_cluster = start_cluster, prev_cluster = start_cluster;

		fs->seekg(fat_offset + METADATA_FAT_OFFSET, fs->beg);
		fs->read((char *)fat_buffer, fat_buffer_size);
		if (!fs->good()) {
			fs->clear();
			return io::status_t::error;
		}

		for (uint32_t i = 0; i < clusters_left; ++i) {
			prev_cluster = found_cluster;
			T fat_entry = fat_buffer[found_cluster - buffer_cluster];
			if constexpr (sizeof(T) == 2) {
				found_cluster = fat_entry < FATX16_BOUNDARY ? fat_entry : (uint32_t)(int16_t)fat_entry;
			} else {
				found_cluster = fat_entry;
			}
			if (!util::in_range(found_cluster, buffer_cluster, buffer_cluster + fat_buffer_size - 1)) {
				buffer_cluster = found_cluster;
				fat_offset = cluster_to_fat_offset(found_cluster, partition_num);
				fs->seekg(fat_offset + METADATA_FAT_OFFSET, fs->beg);
				fs->read((char *)fat_buffer, fat_buffer_size);
				if (!fs->good()) {
					fs->clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
			}
			if (i == (clusters_left - 1)) {
				fat_buffer[prev_cluster - buffer_cluster] = (T)FATX32_CLUSTER_EOC;
				fs->seekp(fat_offset + METADATA_FAT_OFFSET, fs->beg);
				fs->write((const char *)fat_buffer, fat_buffer_size);
				if (!fs->good()) {
					fs->clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
			}
		}

		while (true) {
			prev_cluster = found_cluster;
			T fat_entry = fat_buffer[found_cluster - buffer_cluster];
			if constexpr (sizeof(T) == 2) {
				found_cluster = fat_entry < FATX16_BOUNDARY ? fat_entry : (uint32_t)(int16_t)fat_entry;
			} else {
				found_cluster = fat_entry;
			}
			found_clusters.push_back(prev_cluster);
			++num_of_freed_clusters;
			if (found_cluster == FATX32_CLUSTER_EOC) {
				fs->seekp(fat_offset + METADATA_FAT_OFFSET, fs->beg);
				fs->write((const char *)fat_buffer, fat_buffer_size);
				if (!fs->good()) {
					fs->clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
				break;
			}
			if (!util::in_range(found_cluster, buffer_cluster, buffer_cluster + fat_buffer_size - 1)) {
				fs->seekp(fat_offset + METADATA_FAT_OFFSET, fs->beg);
				fs->write((const char *)fat_buffer, fat_buffer_size);
				if (!fs->good()) {
					fs->clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
				buffer_cluster = found_cluster;
				fat_offset = cluster_to_fat_offset(found_cluster, partition_num);
				fs->seekg(fat_offset + METADATA_FAT_OFFSET, fs->beg);
				fs->read((char *)fat_buffer, fat_buffer_size);
				if (!fs->good()) {
					fs->clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
			}
			fat_buffer[prev_cluster - buffer_cluster] = (T)FATX32_CLUSTER_FREE;
		}

		cluster_free_num[partition_num] += num_of_freed_clusters;

		return io::status_t::success;
	}

	template<typename T>
	static io::status_t
	extend_cluster_chain(uint32_t start_cluster, uint32_t clusters_to_add, unsigned partition_num, std::fstream *fs, std::string_view file_path)
	{
		if (cluster_free_num[partition_num] < clusters_to_add) {
			return io::status_t::full; // not enough free clusters for the dirent stream and/or file/directory
		}

		constexpr uint32_t fat_buffer_size = 4096 / sizeof(T);
		T fat_buffer[fat_buffer_size];
		uint32_t num_of_freed_clusters = 0;
		uint64_t fat_offset = cluster_to_fat_offset(start_cluster, partition_num);
		uint32_t found_cluster = start_cluster, buffer_cluster = start_cluster, old_cluster_num = 0, buffer_offset;

		fs->seekg(fat_offset + METADATA_FAT_OFFSET, fs->beg);
		fs->read((char *)fat_buffer, fat_buffer_size);
		if (!fs->good()) {
			fs->clear();
			return io::status_t::error;
		}

		// Find the eoc of the existing chain
		while (true) {
			buffer_offset = found_cluster - buffer_cluster;
			T fat_entry = fat_buffer[buffer_offset];
			if constexpr (sizeof(T) == 2) {
				found_cluster = fat_entry < FATX16_BOUNDARY ? fat_entry : (uint32_t)(int16_t)fat_entry;
			} else {
				found_cluster = fat_entry;
			}
			if (found_cluster == (T)FATX32_CLUSTER_EOC) {
				break;
			}
			++old_cluster_num;
			if (!util::in_range(found_cluster, buffer_cluster, buffer_cluster + fat_buffer_size - 1)) {
				buffer_cluster = found_cluster;
				fat_offset = cluster_to_fat_offset(found_cluster, partition_num);
				fs->seekg(fat_offset + METADATA_FAT_OFFSET, fs->beg);
				fs->read((char *)fat_buffer, fat_buffer_size);
				if (!fs->good()) {
					fs->clear();
					metadata_set_corrupted_state(partition_num);
					return io::status_t::error;
				}
			}
		}

		std::vector<std::pair<uint32_t, uint32_t>> found_clusters;
		if (io::status_t status = allocate_free_clusters(clusters_to_add, partition_num, fs, found_clusters); status != io::status_t::success) {
			return status;
		}

		// Replace the old eoc with the first cluster found above
		fat_buffer[buffer_offset] = found_clusters[0].first;
		fs->seekg(fat_offset + METADATA_FAT_OFFSET, fs->beg);
		fs->write((const char *)fat_buffer, fat_buffer_size);
		if (!fs->good()) {
			fs->clear();
			metadata_set_corrupted_state(partition_num);
			return io::status_t::error;
		}

		if (io::status_t status = update_cluster_table(found_clusters, partition_num, file_path, fs, old_cluster_num); status != io::status_t::success) {
			return status;
		}
		// No need to write the file's clusters to metadata.bin
		cluster_free_num[partition_num] -= clusters_to_add;
		
		return io::status_t::success;
	}

	static io::status_t
	extend_dirent_stream(uint32_t cluster, char *cluster_buffer, unsigned partition_num, std::fstream *fs)
	{
		assert(last_free_dirent_is_on_boundary && last_dirent_stream_cluster);

		// Mark the new stream as free
		uint64_t bytes_in_cluster = cluster_size[partition_num];
		std::fill_n(cluster_buffer, bytes_in_cluster, FATX_DIRENT_END2);
		fs->seekp(0, fs->end);
		fs->write(cluster_buffer, bytes_in_cluster);
		if (!fs->good()) {
			fs->clear();
			return io::status_t::error;
		}

		// Chain the new cluster to the existing chain of the stream
		uint32_t fat_entry = cluster;
		uint64_t fat_offset = cluster_to_fat_offset(last_dirent_stream_cluster, partition_num);
		fs->seekp(fat_offset + METADATA_FAT_OFFSET, fs->beg);
		fs->write((const char *)&fat_entry, IS_FATX16() ? 2 : 4);
		if (!fs->good()) {
			fs->clear();
			metadata_set_corrupted_state(partition_num);
			return io::status_t::error;
		}

		if (io::status_t status = update_cluster_table(cluster, partition_num, metadata_file_size[partition_num], cluster_t::directory); status != io::status_t::success) {
			return status;
		}

		metadata_file_size[partition_num] += bytes_in_cluster;

		return io::status_t::success;
	}

	template<bool check_is_empty>
	static io::status_t
	scan_dirent_stream(std::string_view remaining_path, std::fstream *fs, unsigned partition_num, DIRENT &io_dirent, uint64_t &dirent_offset, uint32_t start_cluster)
	{
		last_free_dirent_offset = last_found_dirent_offset = 0;

		if constexpr (!check_is_empty) {
			std::string is_root_str(std::filesystem::path("Harddisk/Partition" + std::to_string(partition_num - DEV_PARTITION0) + '/').make_preferred().string());
			if (remaining_path.compare(is_root_str) == 0) {
				// This happens when searching for the root directory
				return io::status_t::is_root_dir;
			}
			if (IS_HDD_HANDLE(partition_num)) {
				constexpr uint64_t length = std::string("Harddisk/PartitionX/").length();
				remaining_path = remaining_path.substr(length);
			}
		}

		uint32_t num_dirent = 0;
		uint64_t bytes_in_cluster = cluster_size[partition_num], num_dirent_per_cluster = bytes_in_cluster >> 6;
		std::unique_ptr<char[]> buffer = std::make_unique_for_overwrite<char[]>(bytes_in_cluster);
		uint32_t dirent_cluster = last_dirent_stream_cluster = start_cluster ? start_cluster : 1; // start from the hint or from the stream of the root directory
		uint32_t pos = 0; // start parsing the path from the very beginning (leading separator is already removed)
		bool is_last_name, found_free_dirent = false;
		while (true) {
			if (!((dirent_cluster - 1) < cluster_tot_num[partition_num])) {
				return io::status_t::corrupt;
			}

			const auto &cluster_info = cluster_to_offset(dirent_cluster, partition_num, fs);
			assert(cluster_info.type == cluster_t::directory);
			if (cluster_info.offset == 0) [[unlikely]] {
				// Trying to find a dirent that was not cached by the metadata file (this should not happen...)
				logger_mod_en(error, io, "Dirent stream at cluster %u was not found in Partition%u.bin file", dirent_cluster, partition_num);
				return io::status_t::error;
			}

			// Read one full cluster
			fs->seekg(cluster_info.offset, fs->beg);
			fs->read(buffer.get(), bytes_in_cluster);
			if (!fs->good()) {
				fs->clear();
				return io::status_t::error;
			}

			// NOTE: pos2 == std::string_view::npos happens when reaching the last name and it's a file
			size_t pos2 = remaining_path.find_first_of(std::filesystem::path::preferred_separator, pos);
			is_last_name = (pos2 == std::string_view::npos) || (pos2 == remaining_path.length());
			std::string_view file_name = remaining_path.substr(pos, is_last_name ? remaining_path.length() : pos2 - pos);
			util::xbox_string_view xbox_file_name = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(file_name);

			// Search the dirent stream until we find the one with the name we are looking for
			uint64_t offset_in_cluster = 0;
			bool dir_found = false;
			while (bytes_in_cluster > 0) {
				if (num_dirent == FATX_MAX_NUM_DIRENT) {
					// Exceeded the maximum number of allowed directories in a single stream
					return io::status_t::corrupt;
				}

				PDIRENT dirent = (PDIRENT)(buffer.get() + offset_in_cluster);
				if ((dirent->name_length == FATX_DIRENT_END1) ||
					(dirent->name_length == FATX_DIRENT_DELETED) ||
					(dirent->name_length == FATX_DIRENT_END2)) {
					if (found_free_dirent == false) {
						last_free_dirent_offset = dirent_offset = cluster_info.offset + offset_in_cluster;
						found_free_dirent = true;
					}
					if (dirent->name_length != FATX_DIRENT_DELETED) {
						// Reached the end of the stream
						// NOTE: clusters are not guaranteed to be aligned on a cluster boundary in metadata.bin files
						last_free_dirent_is_on_boundary = (num_dirent + 1) == num_dirent_per_cluster;
						return check_is_empty ? io::status_t::success : (is_last_name ? io::status_t::name_not_found : io::status_t::path_not_found);
					}
					if constexpr (check_is_empty) {
						++num_dirent;
						bytes_in_cluster -= sizeof(DIRENT);
						offset_in_cluster += sizeof(DIRENT);
						continue;
					}
				}

				if constexpr (check_is_empty) {
					// If we reach here, then we found at least a valid dirent, so the stream is not empty
					return io::status_t::not_empty;
				}

				if ((dirent->name_length == xbox_file_name.length()) &&
					(dirent->name_length <= FATX_MAX_FILE_LENGTH) &&
					(xbox_file_name.compare(0, xbox_file_name.length(), (const char *)(dirent->name), dirent->name_length) == 0)) {
					if (is_last_name) {
						io_dirent.name_length = dirent->name_length;
						io_dirent.attributes = dirent->attributes;
						std::copy_n(dirent->name, dirent->name_length, io_dirent.name);
						io_dirent.first_cluster = dirent->first_cluster;
						io_dirent.size = dirent->size;
						io_dirent.creation_time = dirent->creation_time;
						io_dirent.last_write_time = dirent->last_write_time;
						io_dirent.last_access_time = dirent->last_access_time;
						last_found_dirent_offset = dirent_offset = cluster_info.offset + offset_in_cluster;
						last_dirent_stream_cluster = 0;
						return io::status_t::success;
					} else {
						if (dirent->attributes & FATX_FILE_DIRECTORY) {
							pos = pos2 + 1;
							dirent_cluster = last_dirent_stream_cluster = dirent->first_cluster;
							bytes_in_cluster = cluster_size[partition_num];
							offset_in_cluster = num_dirent = 0;
							found_free_dirent = false;
							dir_found = true;
							break;
						} else {
							// This happens when there is a file with the same name of the directory we are looking for
						}
					}
				}

				++num_dirent;
				bytes_in_cluster -= sizeof(DIRENT);
				offset_in_cluster += sizeof(DIRENT);
			}

			if (dir_found) {
				continue;
			}

			// Attempt to continue the search from a possibly chained stream
			char buffer[4];
			uint32_t fat_entry_size = IS_FATX16() ? 2 : 4;
			uint64_t fat_offset = (dirent_cluster - 1) * fat_entry_size + METADATA_FAT_OFFSET;
			fs->seekg(fat_offset, fs->beg);
			fs->read(buffer, fat_entry_size);
			if (uint32_t found_cluster; !fs->good()) {
				fs->clear();
				return io::status_t::error;
			} else {
				if (fat_entry_size == 2) {
					found_cluster = *(uint16_t *)buffer;
					found_cluster = found_cluster < FATX16_BOUNDARY ? found_cluster : (uint32_t)(int16_t)found_cluster;
				} else {
					found_cluster = *(uint32_t *)buffer;
				}
				assert(found_cluster != FATX32_CLUSTER_FREE);
				if (found_cluster == FATX32_CLUSTER_EOC) {
					// Reached the end of the stream
					// NOTE: clusters are not guaranteed to be aligned on a cluster boundary in metadata.bin files
					last_free_dirent_is_on_boundary = (num_dirent + 1) == num_dirent_per_cluster;
					check_is_empty ? io::status_t::success : (is_last_name ? io::status_t::name_not_found : io::status_t::path_not_found);
				}
				dirent_cluster = last_dirent_stream_cluster = found_cluster;
				bytes_in_cluster = cluster_size[partition_num];
			}
		}
	}

	io::status_t
	find_dirent_for_file(std::string_view remaining_path, std::fstream *fs, unsigned partition_num, DIRENT &io_dirent, uint64_t &dirent_offset)
	{
		return scan_dirent_stream<false>(remaining_path, fs, partition_num, io_dirent, dirent_offset, 0);
	}

	io::status_t
	is_dirent_stream_empty(std::fstream *fs, unsigned partition_num, uint32_t start_cluster)
	{
		uint64_t dummy_offset;
		DIRENT dummy_dirent;
		return scan_dirent_stream<true>("", fs, partition_num, dummy_dirent, dummy_offset, start_cluster);
	}

	io::status_t
	create_dirent_for_file(DIRENT &io_dirent, std::fstream *fs, unsigned partition_num, std::string_view file_path)
	{
		uint64_t bytes_in_cluster = cluster_size[partition_num];
		uint64_t clusters_needed_for_file = io_dirent.attributes & FATX_FILE_DIRECTORY ? 1 : ((io_dirent.size + bytes_in_cluster - 1) & ~(bytes_in_cluster - 1)) >> cluster_shift[partition_num];
		uint64_t clusters_needed_for_dirent_stream = last_free_dirent_is_on_boundary ? 1 : 0;
		uint32_t first_found_cluster = 0;

		// As this is only called when the last file on a path is not found, find_dirent_for_file must have scanned the entire stream and found at least the
		// end of chain dirent (FATX_DIRENT_FREE1/2), which is considered free
		assert(last_free_dirent_offset);

		if ((clusters_needed_for_file == 0) && (clusters_needed_for_dirent_stream == 0)) {
			// This happens when we are creating a file with an initial allocation size of zero, and there is a free slot in the existing dirent stream.
			// In this case, we don't need any new clusters to allocate
			io_dirent.first_cluster = FATX32_CLUSTER_FREE;
			fs->seekp(last_free_dirent_offset, fs->beg);
			fs->write((const char *)&io_dirent, sizeof(DIRENT));
			if (!fs->good()) {
				fs->clear();
				return io::status_t::error;
			}
			return io::status_t::success;
		} else {
			std::unique_ptr<char[]> cluster_buffer = std::make_unique_for_overwrite<char[]>(bytes_in_cluster);
			if (clusters_needed_for_file) {
				// Either we are creating a directory, or a file with a non-zero initial size
				if (clusters_needed_for_dirent_stream) {
					if (cluster_free_num[partition_num] < (clusters_needed_for_file + 1)) {
						return io::status_t::full; // not enough free clusters for the dirent stream and/or file/directory
					}
					// This must search for the file and dirent clusters separately, because they belong to different chains
					std::vector<std::pair<uint32_t, uint32_t>> found_clusters;
					if (io::status_t status = allocate_free_clusters(clusters_needed_for_file, partition_num, fs, found_clusters); status != io::status_t::success) {
						return status;
					}
					// Write the dirent for the created file/directory
					io_dirent.first_cluster = found_clusters[0].first;
					fs->seekp(last_free_dirent_offset, fs->beg);
					fs->write((const char *)&io_dirent, sizeof(DIRENT));
					if (!fs->good()) {
						fs->clear();
						metadata_set_corrupted_state(partition_num);
						return io::status_t::error;
					}
					if (io_dirent.attributes & FATX_FILE_DIRECTORY) {
						std::fill_n(cluster_buffer.get(), bytes_in_cluster, FATX_DIRENT_END2); // write the cluster of the new directory
						fs->seekp(0, fs->end);
						fs->write(cluster_buffer.get(), bytes_in_cluster);
						if (!fs->good()) {
							fs->clear();
							metadata_set_corrupted_state(partition_num);
							return io::status_t::error;
						}
						if (io::status_t status = update_cluster_table(found_clusters[0].first, partition_num, metadata_file_size[partition_num], cluster_t::directory); status != io::status_t::success) {
							return status;
						}
						metadata_file_size[partition_num] += bytes_in_cluster;
					} else {
						// No need to write the file's clusters to metadata.bin
						if (io::status_t status = update_cluster_table(found_clusters, partition_num, file_path, fs); status != io::status_t::success) {
							return status;
						}
					}
					found_clusters.clear();
					if (io::status_t status = allocate_free_clusters(1, partition_num, fs, found_clusters); status != io::status_t::success) {
						return status;
					}
					// Write the cluster used to extend the dirent stream
					if (io::status_t status = extend_dirent_stream(found_clusters[0].first, cluster_buffer.get(), partition_num, fs); status != io::status_t::success) {
						return status;
					}
				} else {
					if (cluster_free_num[partition_num] < clusters_needed_for_file) {
						return io::status_t::full; // not enough free clusters for the file/directory
					}
					std::vector<std::pair<uint32_t, uint32_t>> found_clusters;
					if (io::status_t status = allocate_free_clusters(clusters_needed_for_file, partition_num, fs, found_clusters); status != io::status_t::success) {
						return status;
					}
					// Write the dirent for the created file/directory
					io_dirent.first_cluster = found_clusters[0].first;
					fs->seekp(last_free_dirent_offset, fs->beg);
					fs->write((const char *)&io_dirent, sizeof(DIRENT));
					if (!fs->good()) {
						fs->clear();
						metadata_set_corrupted_state(partition_num);
						return io::status_t::error;
					}
					if (io_dirent.attributes & FATX_FILE_DIRECTORY) {
						std::fill_n(cluster_buffer.get(), bytes_in_cluster, FATX_DIRENT_END2); // write the cluster of the new directory
						fs->seekp(0, fs->end);
						fs->write(cluster_buffer.get(), bytes_in_cluster);
						if (!fs->good()) {
							fs->clear();
							metadata_set_corrupted_state(partition_num);
							return io::status_t::error;
						}
						if (io::status_t status = update_cluster_table(found_clusters[0].first, partition_num, metadata_file_size[partition_num], cluster_t::directory); status != io::status_t::success) {
							return status;
						}
						metadata_file_size[partition_num] += bytes_in_cluster;
					} else {
						// No need to write the file's clusters to metadata.bin
						if (io::status_t status = update_cluster_table(found_clusters, partition_num, file_path, fs); status != io::status_t::success) {
							return status;
						}
					}
				}

				cluster_free_num[partition_num] -= (clusters_needed_for_dirent_stream + clusters_needed_for_file);

				return io::status_t::success;
			}

			if (clusters_needed_for_dirent_stream) {
				if (cluster_free_num[partition_num] == 0) {
					return io::status_t::full; // not enough free clusters to extend the dirent stream
				}
				// This happens when we are creating a file with an initial allocation size of zero, but there isn't a free slot in the existing dirent stream
				std::vector<std::pair<uint32_t, uint32_t>> found_clusters;
				if (io::status_t status = allocate_free_clusters(1, partition_num, fs, found_clusters); status != io::status_t::success) {
					return status;
				}
				// Write the cluster used to extend the dirent stream
				if (io::status_t status = extend_dirent_stream(found_clusters[0].first, cluster_buffer.get(), partition_num, fs); status != io::status_t::success) {
					return status;
				}

				cluster_free_num[partition_num] -= 1;

				return io::status_t::success;
			}

			return io::status_t::error; // unreachable
		}
	}

	io::status_t
	overwrite_dirent_for_file(DIRENT &io_dirent, std::fstream *fs, uint32_t new_size, unsigned partition_num, std::string_view file_path)
	{
		assert(last_found_dirent_offset);

		if (!(io_dirent.attributes & FATX_FILE_DIRECTORY)) {
			// If it is a file, then we must (de)allocate clusters if the new size is different than the old one
			uint32_t bytes_in_cluster = cluster_size[partition_num];
			uint32_t new_cluster_num = ((new_size + bytes_in_cluster - 1) & ~(bytes_in_cluster - 1)) >> cluster_shift[partition_num];
			if (new_size != io_dirent.size) {
				if (new_size > io_dirent.size) {
					if (cluster_free_num[partition_num] < new_cluster_num) {
						return io::status_t::full; // not enough free clusters for the file
					}
					uint32_t old_cluster_num = ((io_dirent.size + bytes_in_cluster - 1) & ~(bytes_in_cluster - 1)) >> cluster_shift[partition_num];
					std::vector<std::pair<uint32_t, uint32_t>> found_clusters;
					if (io::status_t status = allocate_free_clusters(new_cluster_num - old_cluster_num, partition_num, fs, found_clusters); status != io::status_t::success) {
						return status;
					}
					if (io::status_t status = update_cluster_table(found_clusters, partition_num, file_path, fs, old_cluster_num); status != io::status_t::success) {
						return status;
					}
					// No need to write the file's clusters to metadata.bin
					cluster_free_num[partition_num] -= new_cluster_num;
					io_dirent.size = new_size;
				} else if (new_size < io_dirent.size) {
					io::status_t status;
					std::vector<uint32_t> found_clusters;
					if (IS_FATX16()) {
						status = free_allocated_clusters<uint16_t>(io_dirent.first_cluster, new_cluster_num, partition_num, fs, found_clusters);
					} else {
						status = free_allocated_clusters<uint32_t>(io_dirent.first_cluster, new_cluster_num, partition_num, fs, found_clusters);
					}
					if (status != io::status_t::success) {
						return status;
					}
					if (status = update_cluster_table(found_clusters, partition_num); status != io::status_t::success) {
						return status;
					}
					io_dirent.size = new_size;
					io_dirent.first_cluster = new_size ? io_dirent.first_cluster : FATX32_CLUSTER_FREE;
				}
			}
		}

		fs->seekp(last_found_dirent_offset, fs->beg);
		fs->write((const char *)&io_dirent, sizeof(DIRENT)); // overwrite old entry
		if (!fs->good()) {
			fs->clear();
			metadata_set_corrupted_state(partition_num);
			return io::status_t::error;
		}

		return io::status_t::success;
	}

	io::status_t
	delete_dirent_for_file(DIRENT &io_dirent, std::fstream *fs, unsigned partition_num)
	{
		// NOTE: here, we don't need to flush the deleted dirent to the metadata bin file to make it visible to find_dirent_for_file(). This is because
		// the kernel marks files scheduled for deletion and won't allow new create/open request to them

		// Folders can only be deleted if they are empty
		assert(io_dirent.attributes & FATX_FILE_DIRECTORY ? is_dirent_stream_empty(fs, partition_num, io_dirent.first_cluster) == io::status_t::success : true);

		if (io_dirent.first_cluster != FATX32_CLUSTER_FREE) {
			io::status_t status;
			std::vector<uint32_t> found_clusters;
			if (IS_FATX16()) {
				status = free_allocated_clusters<uint16_t>(io_dirent.first_cluster, 0, partition_num, fs, found_clusters);
			} else {
				status = free_allocated_clusters<uint32_t>(io_dirent.first_cluster, 0, partition_num, fs, found_clusters);
			}
			if (status != io::status_t::success) {
				return status;
			}
			if (status = update_cluster_table(found_clusters, partition_num); status != io::status_t::success) {
				return status;
			}
		}

		io_dirent.name_length = FATX_DIRENT_DELETED;
		io_dirent.first_cluster = FATX32_CLUSTER_FREE;

		return io::status_t::success;
	}

	io::status_t
	append_clusters_to_file(DIRENT &io_dirent, std::fstream *fs, int64_t offset, uint32_t size, unsigned partition_num, std::string_view file_path)
	{
		uint64_t cluster_mask = cluster_size[partition_num] - 1;
		uint64_t file_new_size = offset + size;
		uint64_t file_aligned_size = (io_dirent.size + cluster_mask) & ~cluster_mask;

		if (file_new_size > file_aligned_size) {
			assert(IS_HDD_HANDLE(partition_num)); // only device supported right now

			if (io_dirent.first_cluster == FATX32_CLUSTER_FREE) {
				// This happens when writing to an empty file for the very first time

				uint32_t clusters_needed_for_file = ((file_new_size + cluster_mask) & ~cluster_mask) >> cluster_shift[partition_num];
				if (cluster_free_num[partition_num] < clusters_needed_for_file) {
					return io::status_t::full; // not enough free clusters for the file/directory
				}
				std::vector<std::pair<uint32_t, uint32_t>> found_clusters;
				if (io::status_t status = allocate_free_clusters(clusters_needed_for_file, partition_num, fs, found_clusters); status != io::status_t::success) {
					return status;
				}
				io_dirent.first_cluster = found_clusters[0].first;
				if (io::status_t status = update_cluster_table(found_clusters, partition_num, file_path, fs); status != io::status_t::success) {
					return status;
				}
			}
			else {
				// Extend the existing cluster chain

				io::status_t status;
				uint64_t clusters_needed = (((file_new_size + cluster_mask) & ~cluster_mask) - file_aligned_size) >> cluster_shift[partition_num];
				if (IS_FATX16()) {
					status = extend_cluster_chain<uint16_t>(io_dirent.first_cluster, clusters_needed, partition_num, fs, file_path);
				} else {
					status = extend_cluster_chain<uint32_t>(io_dirent.first_cluster, clusters_needed, partition_num, fs, file_path);
				}
				if (status != io::status_t::success) {
					return status;
				}
			}

			io_dirent.size = file_new_size;
		}

		return io::status_t::success;
	}

	static bool
	format_partition(std::fstream *fs, unsigned partition_num, uint32_t cluster_size1)
	{
		cluster_size[partition_num] = cluster_size1 * XBOX_HDD_SECTOR_SIZE;
		cluster_shift[partition_num] = std::countr_zero(cluster_size[partition_num]);
		cluster_tot_num[partition_num] = ((current_partition_table.TableEntries[partition_num - 1 - DEV_PARTITION0].LBASize * XBOX_HDD_SECTOR_SIZE) >> cluster_shift[partition_num]) + 1;
		cluster_free_num[partition_num] = cluster_tot_num[partition_num] - 2;
		metadata_fat_sizes[partition_num] = (cluster_tot_num[partition_num] * (IS_FATX16() ? 2 : 4) + 4095) & ~4095; // align the fat to a page boundary
		metadata_file_size[partition_num] = METADATA_FAT_OFFSET + metadata_fat_sizes[partition_num] + cluster_size[partition_num];
		if (!create_fat(fs, partition_num)) {
			return false;
		}
		if (!create_root_dirent(fs, partition_num)) {
			return false;
		}
		CLUSTER_DATA_ENTRY cluster_data[CLUSTER_TABLE_ELEM_SIZE / sizeof(CLUSTER_DATA_ENTRY)];
		std::fill_n((char *)cluster_data, sizeof(cluster_data), cluster_t::freed);
		cluster_data[1].type = cluster_t::directory;
		cluster_data[1].size = 0;
		cluster_data[1].info = 0;
		cluster_data[1].offset = METADATA_FAT_OFFSET + metadata_fat_sizes[partition_num];
		std::fstream *table_fs = &cluster_table_fs[partition_num];
		table_fs->seekp(0, table_fs->end);
		table_fs->write((const char *)cluster_data, CLUSTER_TABLE_ELEM_SIZE);
		if (!table_fs->good()) {
			return false;
		}
		cluster_table_file_size[partition_num] = CLUSTER_TABLE_ELEM_SIZE;
		last_allocated_cluster[partition_num] = 1;

		return true;
	}

	static bool
	format_partition(std::fstream *fs, unsigned partition_num)
	{
		// NOTE: this overload should only be called at startup by create_partition_metadata_file(), because it resets the partition table and the superblock to their default states

		if (partition_num == DEV_PARTITION0) {
			// NOTE: place partition0_buffer on the heap instead of the stack because it's quite big
			std::unique_ptr<char[]> partition0_buffer = std::make_unique<char[]>(XBOX_HDD_SECTOR_SIZE * XBOX_SWAPPART1_LBA_START);
			std::copy_n(&hdd_partition_table.Magic[0], sizeof(XBOX_PARTITION_TABLE), partition0_buffer.get());
			current_partition_table = hdd_partition_table;
			fs->seekp(0, fs->beg);
			fs->write(partition0_buffer.get(), XBOX_HDD_SECTOR_SIZE * XBOX_SWAPPART1_LBA_START);
			if (!fs->good()) {
				return false;
			}
			return true;
		} else {
			char buffer[4096 * 2] = { 0 };
			PUSER_DATA_AREA user_area = (PUSER_DATA_AREA)buffer;
			user_area->last_cluster_used = 1;
			user_area->is_corrupted = 1;
			user_area->version = METADATA_VERSION_NUM;
			PSUPERBLOCK superblock = (PSUPERBLOCK)&buffer[4096];
			superblock->Signature = FATX_SIGNATURE;
			superblock->VolumeID = 11223344 + partition_num;
			superblock->ClusterSize = 32;
			superblock->RootDirCluster = 1;
			std::fill_n(superblock->Unused, sizeof(superblock->Unused), 0xFF);
			fs->seekp(0, fs->beg);
			fs->write((char *)&buffer, sizeof(buffer));
			if (!fs->good()) {
				return false;
			}
			return format_partition(fs, partition_num, superblock->ClusterSize);
		}
	}

	static bool
	format_partition(std::fstream *fs, unsigned partition_num, const char *superblock1, uint32_t offset, uint32_t size)
	{
		assert(partition_num);

		char buffer[4096 * 2] = { 0 };
		PUSER_DATA_AREA user_area = (PUSER_DATA_AREA)buffer;
		user_area->last_cluster_used = 1;
		user_area->is_corrupted = 1;
		user_area->version = METADATA_VERSION_NUM;
		PSUPERBLOCK superblock = (PSUPERBLOCK)&buffer[4096];
		std::copy_n(superblock1 + offset, size, (char *)superblock);
		fs->seekp(0, fs->beg);
		fs->write((char *)&buffer, sizeof(buffer));
		if (!fs->good()) {
			return false;
		}
		return format_partition(fs, partition_num, superblock->ClusterSize);
	}

	io::status_t
	read_raw_partition(uint64_t offset, uint32_t size, char *buffer, unsigned partition_num, std::fstream *fs)
	{
		if ((partition_num == DEV_PARTITION0) || (offset < metadata_fat_sizes[partition_num])) {
			uint64_t actual_offset = offset;
			if (partition_num == DEV_PARTITION0) {
				assert((offset + size) <= (XBOX_CONFIG_AREA_LBA_SIZE * XBOX_HDD_SECTOR_SIZE));
			}
			else {
				actual_offset += sizeof(USER_DATA_AREA);
			}
			fs->seekg(actual_offset, fs->beg);
			fs->read(buffer, size);
			if (!fs->good()) {
				fs->clear();
				return io::status_t::error;
			}
		}
		else {
			uint32_t bytes_in_cluster = cluster_size[partition_num];
			uint64_t cluster_mask = bytes_in_cluster - 1;
			uint64_t cluster_shift1 = cluster_shift[partition_num];
			uint32_t clusters_spanned = ((offset & cluster_mask) + size + cluster_mask) >> cluster_shift1;
			uint64_t cluster_start = offset & ~cluster_mask;
			uint32_t cluster_end = cluster_start + clusters_spanned, cluster = cluster_start;
			uint64_t cluster_offset = offset - (cluster_start << cluster_shift1);
			uint64_t bytes_left = size;
			uint32_t buffer_offset = 0;

			while (cluster < cluster_end) {
				uint32_t bytes_to_read = std::min(bytes_left, (((uint64_t)cluster + 1) << cluster_shift1) - cluster_offset);
				CLUSTER_INFO_ENTRY info_entry = cluster_to_offset(cluster, partition_num, fs);
				if (info_entry.type == cluster_t::freed) {
					std::fill_n(buffer + buffer_offset, bytes_to_read, 0);
				} else if ((info_entry.type == cluster_t::directory) || (info_entry.type == cluster_t::raw)) {
					fs->seekg(info_entry.offset + cluster_offset, fs->beg);
					fs->read(buffer + buffer_offset, bytes_to_read);
					if (!fs->good()) {
						fs->clear();
						return io::status_t::error;
					}
				} else {
					assert(info_entry.type == cluster_t::file);
					assert(IS_HDD_HANDLE(partition_num)); // only device supported right now
					std::filesystem::path file_path(io::hdd_path);
					file_path /= info_entry.path;
					if (auto opt = open_file(file_path); !opt) {
						return io::status_t::error;
					} else {
						uint64_t file_offset = (uint64_t)info_entry.cluster << cluster_shift1;
						opt->seekg(file_offset + cluster_offset, fs->beg);
						opt->read(buffer + buffer_offset, bytes_to_read);
						if (!opt->good()) {
							if (opt->eof()) {
								// This might happen when reading the last cluster of the file
								uint64_t bytes_read = opt->gcount();
								std::fill_n(buffer + buffer_offset + bytes_read, bytes_to_read - bytes_read, 0);
							} else {
								return io::status_t::error;
							}
						}
					}
				}
				bytes_left -= bytes_to_read;
				buffer_offset += bytes_to_read;
				cluster_offset = 0;
				++cluster;
			}
		}

		return io::status_t::success;
	}

	io::status_t
	write_raw_partition(uint64_t offset, uint32_t size, const char *buffer, unsigned partition_num, std::fstream *fs)
	{
		if (partition_num == DEV_PARTITION0) {
			uint64_t actual_offset = offset;
			if (partition_num == DEV_PARTITION0) {
				assert((offset + size) <= (XBOX_CONFIG_AREA_LBA_SIZE * XBOX_HDD_SECTOR_SIZE));
			}
			else {
				actual_offset += sizeof(USER_DATA_AREA);
			}
			fs->seekg(actual_offset, fs->beg);
			fs->write(buffer, size);
			if (!fs->good()) {
				fs->clear();
				return io::status_t::error;
			}
			if (util::in_range(offset, (uint64_t)0, (uint64_t)sizeof(XBOX_PARTITION_TABLE) - 1)) {
				// If we have written to the partition table, reload our copy of it. We don't reformat all partitions because we expect the homebrew to do it
				fs->seekg(0, fs->beg);
				fs->read((char *)&current_partition_table, sizeof(XBOX_PARTITION_TABLE));
				if (!fs->good()) {
					fs->clear();
					return io::status_t::error;
				}
			}
		}
		else {
			uint32_t bytes_in_cluster = cluster_size[partition_num];
			uint64_t cluster_mask = bytes_in_cluster - 1;
			uint64_t cluster_shift1 = cluster_shift[partition_num];
			uint32_t clusters_spanned = ((offset & cluster_mask) + size + cluster_mask) >> cluster_shift1;
			uint64_t cluster_start = offset & ~cluster_mask;
			uint32_t cluster_end = cluster_start + clusters_spanned, cluster = cluster_start;
			uint64_t cluster_offset = offset - (cluster_start << cluster_shift1);
			uint64_t bytes_left = size;
			uint32_t buffer_offset = 0;

			while (cluster < cluster_end) {
				uint32_t bytes_to_write = std::min(bytes_left, (((uint64_t)cluster + 1) << cluster_shift1) - cluster_offset);
				CLUSTER_INFO_ENTRY info_entry = cluster_to_offset(cluster, partition_num, fs);
				if (info_entry.type == cluster_t::freed) {
					if (cluster_offset) {
						std::unique_ptr<char[]> cluster_buffer = std::make_unique<char[]>(bytes_in_cluster);
						std::copy_n(buffer, bytes_to_write, cluster_buffer.get());
						fs->seekp(0, fs->end);
						fs->write(cluster_buffer.get(), bytes_to_write);
						if (!fs->good()) {
							fs->clear();
							return io::status_t::error;
						}
					} else {
						fs->seekp(0, fs->end);
						fs->write(buffer + buffer_offset, bytes_to_write);
						if (!fs->good()) {
							fs->clear();
							return io::status_t::error;
						}
					}
					if (io::status_t status = update_cluster_table(cluster, partition_num, metadata_file_size[partition_num], cluster_t::raw); status != io::status_t::success) {
						return status;
					}
				} else if ((info_entry.type == cluster_t::directory) || (info_entry.type == cluster_t::raw)) {
					fs->seekp(info_entry.offset + cluster_offset, fs->beg);
					fs->write(buffer + buffer_offset, bytes_to_write);
					if (!fs->good()) {
						fs->clear();
						return io::status_t::error;
					}
				} else {
					assert(info_entry.type == cluster_t::file);
					assert(IS_HDD_HANDLE(partition_num)); // only device supported right now
					std::filesystem::path file_path(io::hdd_path);
					file_path /= info_entry.path;
					if (auto opt = open_file(file_path); !opt) {
						return io::status_t::error;
					} else {
						uint64_t file_offset = (uint64_t)info_entry.cluster << cluster_shift1;
						opt->seekp(file_offset + cluster_offset, fs->beg);
						opt->write(buffer + buffer_offset, bytes_to_write);
						if (!opt->good()) {
							return io::status_t::error;
						}
					}
				}
				bytes_left -= bytes_to_write;
				buffer_offset += bytes_to_write;
				cluster_offset = 0;
				++cluster;
			}

			if (util::in_range(offset, (uint64_t)0, (uint64_t)sizeof(SUPERBLOCK) - 1)) {
				// If we have written to the superblock, reformat the partition
				std::fstream fs0(io::hdd_path /= "Partition0.bin");
				fs0.seekg(0, fs0.beg);
				fs0.read((char *)&current_partition_table, sizeof(XBOX_PARTITION_TABLE));
				if (!fs0.good()) {
					metadata_set_corrupted_state(PARTITION0_HANDLE);
					fs0.clear();
					return io::status_t::error;
				}
				format_partition(fs, partition_num, buffer, offset, sizeof(SUPERBLOCK) - offset);
			}
		}

		return io::status_t::success;
	}

	void
	flush_dirent_for_file(DIRENT &io_dirent, uint64_t dirent_offset, std::fstream *fs, unsigned partition_num)
	{
		fs->seekp(dirent_offset, fs->beg);
		fs->write((const char *)&io_dirent, sizeof(DIRENT));
		if (!fs->good()) {
			fs->clear();
			metadata_set_corrupted_state(partition_num);
		}
	}

	uint64_t
	get_free_cluster_num(unsigned partition_num)
	{
		return cluster_free_num[partition_num];
	}
}

uint64_t
disk_offset_to_partition_offset(uint64_t disk_offset, unsigned &partition_num)
{
	for (const auto &table_entry : fatx::current_partition_table.TableEntries) {
		if (table_entry.Flags & PE_PARTFLAGS_IN_USE) {
			uint64_t base = table_entry.LBAStart * XBOX_HDD_SECTOR_SIZE;
			uint64_t end = (table_entry.LBAStart + table_entry.LBASize) * XBOX_HDD_SECTOR_SIZE - 1;
			if (util::in_range(disk_offset, base, end)) {
				partition_num += (1 + DEV_PARTITION0);
				return disk_offset - base;
			}
		}
	}

	// This must be partition zero, which is not tracked in the partition table
	assert(partition_num == DEV_PARTITION0);
	assert(disk_offset < (XBOX_CONFIG_AREA_LBA_SIZE * XBOX_HDD_SECTOR_SIZE));

	return disk_offset;
}

void
flush_metadata_file(std::fstream *fs, unsigned partition_num)
{
	if (partition_num != DEV_PARTITION0) {
		if (fatx::metadata_is_corrupted[partition_num] == false) {
			USER_DATA_AREA user_area;
			std::fill_n((char *)&user_area, sizeof(USER_DATA_AREA), 0);
			user_area.last_cluster_used = fatx::last_allocated_cluster[partition_num];
			user_area.is_corrupted = 0;
			user_area.version = METADATA_VERSION_NUM;
			fs->seekp(0, fs->beg);
			fs->write((const char *)&user_area, sizeof(USER_DATA_AREA));
			if (!fs->good()) {
				logger_mod_en(error, io, "Failed to flush Partition%u.bin file, it will be recreated on the next launch of nxbx", partition_num);
			}
		}
	}
}

bool
create_partition_metadata_file(std::filesystem::path partition_dir, unsigned partition_num)
{
	// NOTE: this is called from io::init(), so partition_num is zero-based

	unsigned user_partition_num = partition_num;
	partition_num += DEV_PARTITION0;
	std::filesystem::path partition_bin = partition_dir.string() + ".bin";
	fatx::cluster_table_path[partition_num] = partition_dir.string().substr(0, partition_dir.string().length() - 10) + "ClusterTable" + std::to_string(user_partition_num) + ".bin";
	if (!file_exists(partition_bin) || !file_exists(fatx::cluster_table_path[partition_num])) {
		if (auto opt_metadata = create_file(partition_bin); !opt_metadata) {
			return false;
		} else {
			if (user_partition_num) {
				if (auto opt_table = create_file(fatx::cluster_table_path[partition_num]); !opt_table) {
					return false;
				} else {
					fatx::cluster_table_fs[partition_num] = std::move(*opt_table);
				}
			}
			std::fstream &fs = *opt_metadata;
			return fatx::format_partition(&fs, partition_num);
		}
	} else {
		if (user_partition_num) {
			if (auto opt_metadata = open_file(partition_bin); !opt_metadata) {
				return false;
			} else {
				if (auto opt_table = open_file(fatx::cluster_table_path[partition_num]); !opt_table) {
					return false;
				} else {
					std::error_code ec1, ec2;
					fatx::metadata_file_size[partition_num] = std::filesystem::file_size(partition_bin, ec1);
					fatx::cluster_table_file_size[partition_num] = std::filesystem::file_size(fatx::cluster_table_path[partition_num], ec2);
					if (ec1 || ec2) {
						return false;
					}
					char buffer[4096];
					std::fstream &fs = *opt_metadata;
					fs.seekg(0, fs.beg);
					fs.read(buffer, 4096);
					if (!fs.good()) {
						return false;
					}
					fatx::cluster_table_fs[partition_num] = std::move(*opt_table);
					PUSER_DATA_AREA user_area = (PUSER_DATA_AREA)buffer;
					if (user_area->is_corrupted || (user_area->version != METADATA_VERSION_NUM)) {
						if (opt_metadata = create_file(partition_bin), opt_table = create_file(fatx::cluster_table_path[partition_num]); !opt_metadata || !opt_table) {
							return false;
						} else {
							fs = std::move(*opt_metadata);
							fatx::cluster_table_fs[partition_num] = std::move(*opt_table);
							return fatx::format_partition(&fs, partition_num);
						}
					}
					return fatx::setup_cluster_info(&fs, partition_dir, partition_num);
				}
			}
		} else {
			if (auto opt = open_file(partition_bin); !opt) {
				return false;
			} else {
				opt->seekg(0, opt->beg);
				opt->read((char *)&fatx::current_partition_table, sizeof(fatx::XBOX_PARTITION_TABLE));
				if (!opt->good() || std::strncmp((const char *)fatx::current_partition_table.Magic, "****PARTINFO****", std::strlen("****PARTINFO****"))) {
					fatx::current_partition_table = fatx::hdd_partition_table;
				}
			}
		}
	}

	return true;
}
