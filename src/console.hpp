// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "hw/machine.hpp"
#include "io.hpp"
#include "clock.hpp"


class console {
public:
	static console &get()
	{
		static console m_console;
		return m_console;
	}
	console(console const &) = delete;
	void operator=(console const &) = delete;
	bool init(const init_info_t &init_info)
	{
		if (m_is_init == false) {
			if (!((init_info.m_type == console_t::xbox) ||
				(init_info.m_type == console_t::chihiro) ||
				(init_info.m_type == console_t::devkit))) {
				logger_nxbx(error, "Attempted to create unrecognized machine of type %" PRIu32, (uint32_t)init_info.m_type);
				return false;
			}
			timer::init();
			if (!m_machine.init(init_info)) {
				m_machine.deinit();
				return false;
			}
			if (!io::init(init_info.m_nxbx_path, init_info.m_xbe_path, m_machine.get<cpu_t *>())) {
				m_machine.deinit();
				return false;
			}
			m_type = init_info.m_type;
			m_is_init = true;
		}
		return true;
	}
	void deinit()
	{
		io::stop();
		m_machine.deinit();
		m_is_init = false;
	}
	void start() { m_machine.start(); deinit(); }
	void exit() { m_machine.exit(); }
	void apply_log_settings()
	{
		if (m_is_init) {
			m_machine.apply_log_settings();
		}
	}

private:
	console() : m_is_init(false) {}

	machine m_machine;
	bool m_is_init;
	console_t m_type;
};
