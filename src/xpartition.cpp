// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "xpartition.hpp"
#include <array>
#include <algorithm>
#include <assert.h>

#ifdef __GNUC__
// Ignore multichar warning in the fatx signature macro used below
// Requires at least gcc 13 due to a bug in gcc 4.7.0 that cause the pragma to be ignored,
// see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431 for details
#pragma GCC diagnostic ignored "-Wmultichar"
#endif

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

#define FATX16_CLUSTER_FREE (uint16_t)0x0000
#define FATX16_CLUSTER_ROOT (uint16_t)0xFFF8
#define FATX16_CLUSTER_EOC  (uint16_t)0xFFFF
#define FATX32_CLUSTER_FREE (uint32_t)0x00000000
#define FATX32_CLUSTER_ROOT (uint32_t)0xFFFFFFF8
#define FATX32_CLUSTER_EOC  (uint32_t)0xFFFFFFFF

#define FATX_DIRENT_FREE1    0x00
#define FATX_DIRENT_DELETED  0xE5
#define FATX_DIRENT_FREE2    0xFF
#define FATX_MAX_FILE_LENGTH 42

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

	static_assert(sizeof(SUPERBLOCK) == 4096);
	static_assert(sizeof(DIRENT) == 64);

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

	static uint64_t cluster_size[XBOX_NUM_OF_PARTITIONS]; // in bytes

	static constexpr uintmax_t metadata_fat_sizes[XBOX_NUM_OF_PARTITIONS] = {
		0,             // don't use
		1228 * 1024,   // partition1
		68 * 1024,     // partition2
		100 * 1024,    // partition3
		100 * 1024,    // partition4
		100 * 1024,    // partition5
	};

	static bool
	setup_cluster_size(std::fstream *fs, unsigned partition_num)
	{
		assert(partition_num);
		assert(partition_num < XBOX_NUM_OF_PARTITIONS);

		char buffer[sizeof(SUPERBLOCK::ClusterSize)];
		fs->seekg(offsetof(SUPERBLOCK, ClusterSize), fs->beg);
		fs->read(buffer, sizeof(buffer));
		if (!fs->good()) {
			return false;
		}
		cluster_size[partition_num] = *(uint32_t *)buffer * XBOX_HDD_SECTOR_SIZE;

		return true;
	}

	static bool
	create_root_dirent(std::fstream *fs, unsigned partition_num)
	{
		assert(partition_num);
		assert(partition_num < XBOX_NUM_OF_PARTITIONS);

		uint64_t size = cluster_size[partition_num];
		std::unique_ptr<char[]> buffer = std::make_unique_for_overwrite<char[]>(size);
		std::fill_n(buffer.get(), size, FATX_DIRENT_FREE2);

		fs->seekp(metadata_fat_sizes[partition_num], fs->beg);
		fs->write(buffer.get(), size);
		return fs->good();
	}

	static bool
	create_fat(std::fstream *fs, std::filesystem::path partition_dir, unsigned partition_num)
	{
		assert(partition_num);
		assert(partition_num < XBOX_NUM_OF_PARTITIONS);

		std::string partition0_dir = partition_dir.string().substr(0, partition_dir.string().length() - 1);
		partition0_dir += "0.bin";
		uint64_t partition_length, fat_length;
		if (auto opt = open_file(partition0_dir); !opt) {
			partition_length = hdd_partition_table.TableEntries[partition_num - 1].LBASize * XBOX_HDD_SECTOR_SIZE;
		} else {
			XBOX_PARTITION_TABLE::TABLE_ENTRY table_entry[14];
			opt->seekg(offsetof(XBOX_PARTITION_TABLE, TableEntries), opt->beg);
			opt->read((char *)&table_entry, sizeof(table_entry));
			if (!opt->good()) {
				return false;
			} else {
				partition_length = table_entry[partition_num - 1].LBASize * XBOX_HDD_SECTOR_SIZE;
			}
		}

		if (partition_length) {
			// NOTE: this assumes that the non-standard partitions are bigger than around 1GiB (fatx16/32 size boundary)
			bool is_fatx16;
			fat_length = partition_length / cluster_size[partition_num];
			if ((partition_num >= 2) && (partition_num <= 5)) {
				fat_length <<= 1;
				is_fatx16 = true;
			} else {
				fat_length <<= 2;
				is_fatx16 = false;
			}

			fat_length = (fat_length + 4095) & ~4095; // align the fat to a page boundary
			std::error_code ec;
			std::filesystem::resize_file(partition_dir.string() + ".bin", sizeof(SUPERBLOCK) + fat_length, ec);
			if (ec) {
				return false;
			}

			// NOTE: place fat_buffer on the heap instead of the stack because it can become quite big, depending on the size of the partition. For example, in many
			// homebrews the non-standard partitions can be up to 927GiB. Assuming a cluster size of 64KiB, this will result in a fat of around 58MiB
			std::unique_ptr<char[]> fat_buffer = std::make_unique_for_overwrite<char[]>(fat_length);
			std::fill(&fat_buffer[0], &fat_buffer[0] + fat_length, 0); // mark all clusters as free
			if (is_fatx16) {
				uint16_t *fatx16_buffer = (uint16_t *)&fat_buffer[0];
				fatx16_buffer[0] = FATX16_CLUSTER_ROOT;
				fatx16_buffer[1] = FATX16_CLUSTER_EOC;
			} else {
				uint32_t *fatx32_buffer = (uint32_t *)&fat_buffer[0];
				fatx32_buffer[0] = FATX32_CLUSTER_ROOT;
				fatx32_buffer[1] = FATX32_CLUSTER_EOC;
			}

			fs->seekp(sizeof(SUPERBLOCK), fs->beg);
			fs->write(fat_buffer.get(), fat_length);
			if (!fs->good()) {
				return false;
			}

			return true;
		}

		return false;
	}
}

bool
create_partition_metadata_file(std::filesystem::path partition_dir, unsigned partition_num)
{
	std::filesystem::path partition_bin = partition_dir.string() + ".bin";
	if (!file_exists(partition_bin)) {
		if (auto opt = create_file(partition_bin); !opt) {
			return false;
		} else {
			if (partition_num == 0) {
				// NOTE: place partition0_buffer on the heap instead of the stack because it's quite big
				std::unique_ptr<char[]> partition0_buffer = std::make_unique<char[]>(XBOX_HDD_SECTOR_SIZE * XBOX_SWAPPART1_LBA_START);
				std::copy_n(&fatx::hdd_partition_table.Magic[0], sizeof(fatx::XBOX_PARTITION_TABLE), partition0_buffer.get());
				std::fstream &fs = opt.value();
				fs.seekp(0, fs.beg);
				fs.write(partition0_buffer.get(), XBOX_HDD_SECTOR_SIZE * XBOX_SWAPPART1_LBA_START);
				if (!fs.good()) {
					return false;
				}
			} else {
				fatx::SUPERBLOCK superblock{};
				superblock.Signature = FATX_SIGNATURE;
				superblock.VolumeID = 11223344 + partition_num;
				superblock.ClusterSize = 32;
				superblock.RootDirCluster = 1;
				std::fill_n(superblock.Unused, sizeof(superblock.Unused), 0xFF);
				std::fstream &fs = opt.value();
				fs.seekp(0, fs.beg);
				fs.write((char *)&superblock, sizeof(fatx::SUPERBLOCK));
				if (!fs.good()) {
					return false;
				}
				fatx::cluster_size[partition_num] = superblock.ClusterSize * XBOX_HDD_SECTOR_SIZE;
				if (!fatx::create_fat(&fs, partition_dir, partition_num)) {
					return false;
				}
				if (!fatx::create_root_dirent(&fs, partition_num)) {
					return false;
				}
			}
		}
	} else {
		if (partition_num) {
			if (auto opt = open_file(partition_bin); !opt) {
				return false;
			} else {
				std::fstream &fs = opt.value();
				if (!fatx::setup_cluster_size(&fs, partition_num)) {
					return false;
				}
				std::error_code ec;
				uintmax_t size = std::filesystem::file_size(partition_bin, ec);
				if (ec) {
					return false;
				} else if (size == sizeof(fatx::SUPERBLOCK)) {
					// This is a legacy partition.bin file that lacks the FAT after the superblock
					if (!fatx::create_fat(&fs, partition_dir, partition_num)) {
						return false;
					}
					if (!fatx::create_root_dirent(&fs, partition_num)) {
						return false;
					}
				} else if (size == fatx::metadata_fat_sizes[partition_num]) {
					// This is a legacy partition.bin file that lacks the root dirent after the FAT
					if (!fatx::create_root_dirent(&fs, partition_num)) {
						return false;
					}
				}
			}
		}
	}

	return true;
}
