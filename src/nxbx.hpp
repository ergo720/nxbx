// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>
#include <cinttypes>
#include <optional>
#include <assert.h>
#include <vector>
#include "logger.hpp"
#include "isettings.hpp"

#define nxbx_mod_fatal(mod, msg, ...) do { nxbx::fatal(log_module::mod, msg __VA_OPT__(,) __VA_ARGS__); } while(0)
#define nxbx_fatal(msg, ...) nxbx_mod_fatal(MODULE_NAME, msg __VA_OPT__(,) __VA_ARGS__)


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

enum class input_t : uint32_t {
	xbe,
	xiso,
};

struct init_info_t {
	std::string m_kernel_path;
	std::string m_nxbx_path;
	std::string m_input_path;
	std::string m_keys_path;
	disas_syntax m_syntax;
	uint32_t m_use_dbg;
	console_t m_console_type;
	input_t m_input_type;
	int32_t m_sync_part;
};

namespace nxbx {
	bool init_console(const init_info_t &init_info);
	bool validate_input_file(init_info_t &init_info, std::string_view arg_str);
	bool init_settings(const init_info_t &init_info);
	void save_settings();
	isettings *get_settings();
	void update_logging();
	void start();
	void exit();
	const std::string &console_to_string(console_t type);
	void fatal(log_module name, const char *msg, ...);
}
