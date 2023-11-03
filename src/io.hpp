// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>


inline bool pending_packets = false;

bool io_init(const char *nxbx_path, const char *xbe_path);
void io_stop();
void submit_io_packet(uint32_t addr);
void flush_pending_packets();
uint32_t query_io_packet(uint32_t id, bool query_status);
