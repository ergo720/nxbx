// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "util.hpp"


struct cpu_t;

namespace io {
	inline bool pending_packets = false;
	inline util::xbox_string xbe_name;
	inline util::xbox_string xbe_path;

	bool init(std::string nxbx_path, std::string xbe_path_, cpu_t *cpu);
	void stop();
	void submit_io_packet(uint32_t addr);
	void flush_pending_packets();
	void query_io_packet(uint32_t addr);
}
