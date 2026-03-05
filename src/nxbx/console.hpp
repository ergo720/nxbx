// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "machine.hpp"
#include "host.hpp"
#include <atomic>
#include <thread>


enum class console_state {
	shut_down,
	stopping,
	running,
	initialized,
};

enum class console_t : uint32_t {
	xbox,
	chihiro,
	devkit,
};

class console {
public:
	console(const boot_params &params);

	void start();
	bool exit(bool wait = false);

	console_state get_state() { return m_state; }
	boot_params get_boot_params() { return m_params; }
	void apply_log_settings();
	void update_tray_state(tray_state state, bool do_int);
	static const std::string &to_string(console_t type);

private:
	void cpu_thread();
	void deinit();

	machine m_machine;
	std::atomic<console_state> m_state;
	boot_params m_params;
	std::jthread m_cpu_thr;
};

extern console* g_console;
