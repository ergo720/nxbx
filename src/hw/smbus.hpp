// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <optional>
#include "../logger.hpp"


class machine;

class smbus_device {
public:
	smbus_device(log_module module_name) : m_log_module(module_name) {}
	virtual void deinit() = 0;
	virtual std::optional<uint16_t> quick_command(bool command) { logger<log_lv::warn, true>(m_log_module, "Unhandled quick command"); return std::nullopt; }
	virtual std::optional<uint16_t> receive_byte() { logger<log_lv::warn, true>(m_log_module, "Unhandled receive command"); return std::nullopt; }
	virtual std::optional<uint16_t> send_byte(uint8_t data) { logger<log_lv::warn, true>(m_log_module, "Unhandled send command"); return std::nullopt; }
	virtual std::optional<uint16_t> read_byte(uint8_t command) { logger<log_lv::warn, true>(m_log_module, "Unhandled read byte command"); return std::nullopt; }
	virtual std::optional<uint16_t> write_byte(uint8_t command, uint8_t data) { logger<log_lv::warn, true>(m_log_module, "Unhandled write byte command"); return std::nullopt; }
	virtual std::optional<uint16_t> read_word(uint8_t command) { logger<log_lv::warn, true>(m_log_module, "Unhandled read word command"); return std::nullopt; }
	virtual std::optional<uint16_t> write_word(uint8_t command, uint16_t data) { logger<log_lv::warn, true>(m_log_module, "Unhandled write word command"); return std::nullopt; }
	virtual std::optional<uint16_t> process_call(uint8_t command, uint16_t data) { logger<log_lv::warn, true>(m_log_module, "Unhandled process call command"); return std::nullopt; }

protected:
	log_module m_log_module;
};

class smbus {
public:
	smbus(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	uint16_t read16(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t data);
	template<bool log = false>
	void write16(uint32_t addr, const uint16_t data);

private:
	bool update_io(bool is_update);
	void start_cycle();

	machine *const m_machine;
	uint8_t m_regs[16];
	uint8_t m_block_data[32];
	unsigned m_block_off;
	std::unordered_map<uint8_t, smbus_device *> m_devs;
};
