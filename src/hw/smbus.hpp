// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <stdint.h>
#include <unordered_map>
#include "../logger.hpp"

#define SMBUS_GS_addr 0xC000
#define SMBUS_GE_addr 0xC002
#define SMBUS_HA_addr 0xC004
#define SMBUS_HD0_addr 0xC006
#define SMBUS_HD1_addr 0xC007
#define SMBUS_HC_addr 0xC008
#define SMBUS_HB_addr 0xC009
#define SMBUS_REG_off(x) ((x) - SMBUS_GS_addr)

class machine;

class smbus_device {
public:
protected:
	log_module m_log_module;
	uint8_t m_cmd_status; // 0=success, non-zero=error
	void set_cmd_failed() { m_cmd_status |= 1; }

public:
	smbus_device(log_module module_name) : m_log_module(module_name), m_cmd_status(0) {}
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

class smbus {
public:
	smbus(machine *machine) : m_machine(machine) {}
	bool init();
	void deinit();
	void reset();
	void update_io_logging() { update_io(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	uint16_t read16(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);
	template<bool log = false>
	void write16(uint32_t addr, const uint16_t value);

private:
	enum cycle_type {
		quick_command,
		byte_command,
		word_command,
		block_command,
	};

	bool update_io(bool is_update);
	void start_cycle();
	template<cycle_type cmd, bool is_read, typename T = uint8_t>
	void end_cycle(smbus_device *dev, T value = 0);

	machine *const m_machine;
	uint8_t m_regs[16];
	uint8_t m_block_data[32];
	unsigned m_block_off;
	std::unordered_map<uint8_t, smbus_device *> m_devs;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ SMBUS_GS_addr, "STATUS" },
		{ SMBUS_GE_addr, "CONTROL" },
		{ SMBUS_HA_addr, "ADDRESS" },
		{ SMBUS_HD0_addr, "DATA0" },
		{ SMBUS_HD1_addr, "DATA1" },
		{ SMBUS_HC_addr, "COMMAND" },
		{ SMBUS_HB_addr, "FIFO" },
	};
};
