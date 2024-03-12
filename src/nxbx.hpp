// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>
#include <cinttypes>
#include <assert.h>
#include "logger.hpp"


enum class disas_syntax : uint32_t {
	att,
	masm,
	intel,
};

enum class console_t : uint32_t {
	xbox,
	chihiro,
	devkit,
};

struct init_info_t {
	std::string m_kernel;
	std::string m_nxbx_path;
	std::string m_xbe_path;
	disas_syntax m_syntax;
	uint32_t m_use_dbg;
	console_t m_type;
};

// Settings struct declarations, used in the settings class
struct core_s {
	uint32_t version;
	int64_t sys_time_bias;
};

namespace nxbx {
	bool init_console(const init_info_t &init_info);
	bool init_settings(const init_info_t &init_info);
	void save_settings();
	template<typename T>
	T &get_settings();
	void start();
	void exit();
	void fatal(const char *msg, ...);
	const std::string &console_to_string(console_t type);
}
