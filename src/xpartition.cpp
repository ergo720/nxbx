// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "xpartition.hpp"
#include <array>

#define PE_PARTFLAGS_IN_USE	0x80000000

// NOTE: the sizes below are in sector units, one sector == 512 bytes
#define XBOX_HDD_SECTOR_SIZE        512
#define XBOX_SWAPPART_LBA_SIZE      0x177000
#define XBOX_SWAPPART1_LBA_START    0x400
#define XBOX_SWAPPART2_LBA_START    (XBOX_SWAPPART1_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SWAPPART3_LBA_START    (XBOX_SWAPPART2_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SYSPART_LBA_START      (XBOX_SWAPPART3_LBA_START + XBOX_SWAPPART_LBA_SIZE)
#define XBOX_SYSPART_LBA_SIZE       0xfa000
#define XBOX_MUSICPART_LBA_START    (XBOX_SYSPART_LBA_START + XBOX_SYSPART_LBA_SIZE)
#define XBOX_MUSICPART_LBA_SIZE     0x9896b0
#define XBOX_CONFIG_AREA_LBA_START  0
#define XBOX_CONFIG_AREA_LBA_SIZE   XBOX_SWAPPART1_LBA_START

#define FATX_NAME_LENGTH 32
#define FATX_ONLINE_DATA_LENGTH 2048
#define FATX_RESERVED_LENGTH 1968
#define FATX_SIGNATURE 'XTAF'


#pragma pack(1)
struct XBOX_PARTITION_TABLE {
	uint8_t Magic[16];
	int8_t Res0[32];
	struct {
		uint8_t Name[16];
		uint32_t Flags;
		uint32_t LBAStart;
		uint32_t LBASize;
		uint32_t Reserved;
	} TableEntries[14];
};

struct FATX_SUPERBLOCK {
	uint32_t Signature;
	uint32_t VolumeID;
	uint32_t ClusterSize;
	uint32_t RootDirCluster;
	uint16_t Name[FATX_NAME_LENGTH];
	uint8_t OnlineData[FATX_ONLINE_DATA_LENGTH];
	uint8_t Unused[FATX_RESERVED_LENGTH];
};
using PFATX_SUPERBLOCK = FATX_SUPERBLOCK *;
#pragma pack()

/*
Drive Letter  Description  Offset (bytes)  Size (bytes)  Filesystem       Device Object
N/A           Config Area  0x00000000      0x00080000    Fixed Structure  \Device\Harddisk0\Partition0
X             Game Cache   0x00080000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition3
Y             Game Cache   0x2ee80000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition4
Z             Game Cache   0x5dc80000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition5
C             System       0x8ca80000      0x1f400000    FATX 	          \Device\Harddisk0\Partition2
E             Data         0xabe80000      0x131f00000   FATX 	          \Device\Harddisk0\Partition1
*/
// Note that this table ignores the non-standard partitions with drive letters F: and G:

static constexpr XBOX_PARTITION_TABLE hdd_partition_table = {
	{ '*', '*', '*', '*', 'P', 'A', 'R', 'T', 'I', 'N', 'F', 'O', '*', '*', '*', '*' },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{
		{ { 'X', 'B', 'O', 'X', ' ', 'S', 'H', 'E', 'L', 'L', ' ', ' ', ' ', ' ', ' ', ' ' }, PE_PARTFLAGS_IN_USE, XBOX_MUSICPART_LBA_START, XBOX_MUSICPART_LBA_SIZE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'D', 'A', 'T', 'A', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, PE_PARTFLAGS_IN_USE, XBOX_SYSPART_LBA_START, XBOX_SYSPART_LBA_SIZE, 0 },
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


bool
create_partition_metadata_file(std::filesystem::path partition_dir, unsigned partition_num)
{
	std::filesystem::path partition_bin = partition_dir.string() + ".bin";
	if (!file_exists(partition_bin)) {
		if (auto opt = create_file(partition_bin); !opt) {
			return false;
		}
		else {
			if (partition_num == 0) {
				// NOTE: place partition0_buffer on the heap instead of the stack because it's quite big
				std::unique_ptr<char[]> partition0_buffer = std::make_unique<char[]>(XBOX_HDD_SECTOR_SIZE * XBOX_SWAPPART1_LBA_START);
				std::copy_n(&hdd_partition_table.Magic[0], sizeof(XBOX_PARTITION_TABLE), partition0_buffer.get());
				std::fstream &fs = opt.value();
				fs.seekg(0, fs.beg);
				fs.write(partition0_buffer.get(), XBOX_HDD_SECTOR_SIZE * XBOX_SWAPPART1_LBA_START);
				if (fs.rdstate() != std::ios_base::goodbit) {
					return false;
				}
			}
			else {
				FATX_SUPERBLOCK superblock{};
				superblock.Signature = FATX_SIGNATURE;
				superblock.VolumeID = 11223344 + partition_num;
				superblock.ClusterSize = 32;
				superblock.RootDirCluster = 1;
				std::fill_n(superblock.Unused, sizeof(superblock.Unused), 0xFF);
				std::fstream &fs = opt.value();
				fs.seekg(0, fs.beg);
				fs.write((char *)&superblock, sizeof(FATX_SUPERBLOCK));
				if (fs.rdstate() != std::ios_base::goodbit) {
					return false;
				}
			}
		}
	}

	return true;
}
