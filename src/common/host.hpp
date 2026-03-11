// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <expected>
#include "logger.hpp"

#define nxbx_mod_fatal(mod, msg, ...) do { Host::Fatal(log_module::mod, msg __VA_OPT__(,) __VA_ARGS__); } while(0)
#define nxbx_fatal(msg, ...) nxbx_mod_fatal(MODULE_NAME, msg __VA_OPT__(,) __VA_ARGS__)


enum class console_t : uint32_t;

enum class input_t : uint32_t {
	xbe,
	xiso,
	invalid,
};

enum class disas_syntax : uint32_t {
	att,
	masm,
	intel,
};

struct init_info_t {
	std::string kernel_path;
	std::string nxbx_dir;
	std::string input_path;
	std::string keys_path;
	disas_syntax syntax;
	uint32_t use_dbg;
	console_t console_type;
	input_t input_type;
	int32_t sync_part;
};

struct boot_params {
	disas_syntax syntax;
	uint32_t use_dbg;
	console_t console_type;
};

namespace Host
{
	// Prevents multiple shutdown requests during a single emulation session
	inline std::atomic_bool g_shutdown_requested = false;

	//  Checks if the input file is known
	std::expected<input_t, std::string> validate_input_file(std::string_view arg_str);

	// Default theme name for the platform
	const char *GetDefaultThemeName();

	// Attempts to setup kernel file path
	std::string SetupKernelPath(std::string kernel_path);

	// Terminates the emulation
	void Fatal(log_module name, const char *msg, ...);

	// Requests shut down of the current machine
	void RequestShutdown(bool allow_confirm, bool allow_save_to_state, bool default_save_to_state);

	// Signals the machine has started
	void SignalStartup();

	// Signals the machine has stopped
	void SignalStop();

	// Checks if the user started in no gui mode
	bool InNoGUIMode();
}
