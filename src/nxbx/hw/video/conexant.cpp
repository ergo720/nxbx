// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"
#include "conexant.hpp"

#define MODULE_NAME conexant


/** Private device implementation **/
class conexant::Impl
{
public:
	void init();
	void reset();
	void quick_command(bool command);
	uint8_t receive_byte();
	void send_byte(uint8_t value);
	uint8_t read_byte(uint8_t command);
	void write_byte(uint8_t command, uint8_t value);
	uint16_t read_word(uint8_t command);
	void write_word(uint8_t command, uint16_t value);

private:
	uint8_t m_regs[256];
};

void conexant::Impl::quick_command(bool command)
{
	logger_en(info, "%s, ignored", __func__);
	return;
}

uint8_t conexant::Impl::receive_byte()
{
	logger_en(info, "%s, ignored", __func__);
	return 0;
}

void conexant::Impl::send_byte(uint8_t value)
{
	logger_en(info, "%s, ignored", __func__);
	return;
}

uint8_t conexant::Impl::read_byte(uint8_t command)
{
	return m_regs[command];
}

void conexant::Impl::write_byte(uint8_t command, uint8_t value)
{
	m_regs[command] = value;
}

uint16_t conexant::Impl::read_word(uint8_t command)
{
	return m_regs[command] | ((uint16_t)m_regs[command + 1] << 8);
}

void conexant::Impl::write_word(uint8_t command, uint16_t value)
{
	m_regs[command] = value & 0xFF;
	m_regs[command + 1] = value >> 8;
}

void conexant::Impl::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
}

void conexant::Impl::init()
{
	reset();
}

/** Public interface implementation **/
void conexant::init(machine *machine, log_module log_module)
{
	m_log_module = log_module;
	m_impl->init();
}

void conexant::deinit()
{
	// empty
}

void conexant::reset()
{
	m_impl->reset();
}

void conexant::quick_command(bool command)
{
	return m_impl->quick_command(command);
}

uint8_t conexant::receive_byte()
{
	return m_impl->receive_byte();
}

void conexant::send_byte(uint8_t value)
{
	m_impl->send_byte(value);
}

uint8_t conexant::read_byte(uint8_t command)
{
	return m_impl->read_byte(command);
}

void conexant::write_byte(uint8_t command, uint8_t value)
{
	m_impl->write_byte(command, value);
}

uint16_t conexant::read_word(uint8_t command)
{
	return m_impl->read_word(command);
}

void conexant::write_word(uint8_t command, uint16_t value)
{
	m_impl->write_word(command, value);
}

conexant::conexant() : m_impl{std::make_unique<conexant::Impl>()} {}
conexant::~conexant() {}

