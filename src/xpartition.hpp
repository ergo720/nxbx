// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "files.hpp"

#define XBOX_NUM_OF_PARTITIONS 6


bool create_partition_metadata_file(std::filesystem::path partition_dir, unsigned partition_num);
