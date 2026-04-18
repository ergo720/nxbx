// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#pragma once

#include "logger.hpp"


class machine;

// Abstract class from which all devices connected to the smbus derive from
class smbus_device
{
protected:
	log_module m_log_module;
	uint8_t m_cmd_status = 0; // 0=success, non-zero=error
	void set_cmd_failed() { m_cmd_status |= 1; }

public:
	virtual void init(machine *machine, log_module module_name) = 0;
	virtual void deinit() = 0;
	virtual void quick_command(bool command) { logger<log_lv::warn, true>(m_log_module, "Unhandled quick command"); set_cmd_failed(); }
	virtual uint8_t receive_byte() { logger<log_lv::warn, true>(m_log_module, "Unhandled receive command"); set_cmd_failed(); return 0; }
	virtual void send_byte(uint8_t value) { logger<log_lv::warn, true>(m_log_module, "Unhandled send command"); set_cmd_failed(); }
	virtual uint8_t read_byte(uint8_t command) { logger<log_lv::warn, true>(m_log_module, "Unhandled read byte command"); set_cmd_failed(); return 0; }
	virtual void write_byte(uint8_t command, uint8_t value) { logger<log_lv::warn, true>(m_log_module, "Unhandled write byte command"); set_cmd_failed(); }
	virtual uint16_t read_word(uint8_t command) { logger<log_lv::warn, true>(m_log_module, "Unhandled read word command"); set_cmd_failed(); return 0; }
	virtual void write_word(uint8_t command, uint16_t value) { logger<log_lv::warn, true>(m_log_module, "Unhandled write word command"); set_cmd_failed(); }
	virtual uint16_t process_call(uint8_t command, uint16_t value) { logger<log_lv::warn, true>(m_log_module, "Unhandled process call command"); set_cmd_failed(); return 0; }
	bool has_cmd_succeeded() { return m_cmd_status == 0; }
	void clear_cmd_status() { m_cmd_status = 0; }
};
