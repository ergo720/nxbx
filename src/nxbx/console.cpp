// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#include "console.hpp"
#include "io.hpp"
#include "clock.hpp"


console* g_console = nullptr;

static const std::string s_console_xbox_string("xbox");
static const std::string s_console_chihiro_string("chihiro");
static const std::string s_console_devkit_string("devkit");
static const std::string s_console_unknown_string("unknown");

console::console(const boot_params &params)
{
	m_state = console_state::shut_down;
	m_params = params;

	if (!((params.console_type == console_t::xbox) ||
		(params.console_type == console_t::chihiro) ||
		(params.console_type == console_t::devkit))) {
		logger_mod_en(error, nxbx, "Attempted to create unrecognized machine of type %" PRIu32, std::to_underlying<console_t>(params.console_type));
		return;
	}
	timer::init();
	if (!m_machine.init(params)) {
		m_machine.deinit();
		return;
	}
	io::init(m_machine.get<cpu_t *>());
	m_state = console_state::initialized;
}

void console::deinit()
{
	io::stop();
	m_machine.deinit();
	m_state = console_state::shut_down;
	Host::SignalStop();
}

void console::start()
{
	if (m_state == console_state::initialized) {
		Host::SignalStartup();
		m_cpu_thr = std::jthread(std::bind_front(&console::cpu_thread, this));
	}
}

bool console::exit(bool wait)
{
	bool sent = m_state == console_state::running ? true : false;
	if (sent) {
		m_state = console_state::stopping;
		m_machine.exit();

		// Also inform the UI thread that we are about to terminate
		Host::RequestShutdown(false, false, false);

		if (wait) {
			m_cpu_thr.join();
		}
	}
	else if (m_state == console_state::initialized) {
		deinit();
	}
	return sent;
}

void console::cpu_thread()
{
	m_state = console_state::running;
	m_machine.start();
	m_state = console_state::stopping;
	deinit();
}

void console::apply_log_settings()
{
	if ((m_state == console_state::running) || (m_state == console_state::initialized)) {
		m_machine.apply_log_settings();
	}
}

void console::update_tray_state(tray_state state, bool do_int)
{
	if ((m_state == console_state::running) || (m_state == console_state::initialized)) {
		m_machine.get<smc>().update_tray_state(state, do_int);
	}
}

const std::string &console::to_string(console_t type)
{
	switch (type)
	{
	case console_t::xbox:
		return s_console_xbox_string;

	case console_t::chihiro:
		return s_console_chihiro_string;

	case console_t::devkit:
		return s_console_devkit_string;

	default:
		return s_console_unknown_string;
	}
}
