// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME conexant


std::optional<uint16_t>
conexant::quick_command(bool command)
{
	return 0;
}

std::optional<uint16_t>
conexant::receive_byte()
{
	return 0;
}

std::optional<uint16_t>
conexant::send_byte(uint8_t value)
{
	return 0;
}

std::optional<uint16_t>
conexant::read_byte(uint8_t command)
{
	return m_regs[command];
}

std::optional<uint16_t>
conexant::write_byte(uint8_t command, uint8_t value)
{
	m_regs[command] = value;
	return 0;
}

std::optional<uint16_t>
conexant::read_word(uint8_t command)
{
	return m_regs[command] | ((uint16_t)m_regs[command + 1] << 8);
}

std::optional<uint16_t>
conexant::write_word(uint8_t command, uint16_t value)
{
	m_regs[command] = value & 0xFF;
	m_regs[command + 1] = value >> 8;
	return 0;
}

void
conexant::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
}

bool
conexant::init()
{
	reset();
	return true;
}
