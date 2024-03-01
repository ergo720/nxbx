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

class init_info_t {
public:
	std::string m_kernel;
	std::string m_nxbx_path;
	std::string m_xbe_path;
	disas_syntax m_syntax;
	uint32_t m_use_dbg;
	console_t m_type;
};

namespace nxbx {
	bool init_console(const init_info_t &init_info);
	void start();
	void fatal(const char *msg, ...);
	std::string_view console_to_string(console_t type);
}
