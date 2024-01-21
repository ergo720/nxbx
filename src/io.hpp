// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <string>


inline bool pending_packets = false;

bool io_init(std::string nxbx_path, std::string xbe_path);
void io_stop();
void submit_io_packet(uint32_t addr);
void flush_pending_packets();
void query_io_packet(uint32_t addr);
