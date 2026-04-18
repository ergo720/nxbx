// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720

#include "eeprom.hpp"
#include "files.hpp"
#include "paths.hpp"
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <fstream>

#define MODULE_NAME eeprom


// This is bunnie's eeprom, except that it stores the encrypted settings unencrypted, because nboxkrnl cannot decrypt them yet
static constexpr uint8_t s_default_eeprom[] = {
	0xe3, 0x1c, 0x5c, 0x23, 0x6a, 0x58, 0x68, 0x37,
	0xb7, 0x12, 0x26, 0x6c, 0x99, 0x11, 0x30, 0xd1,
	0xe2, 0x3e, 0x4d, 0x56, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x0b, 0x84, 0x44, 0xed, 0x31, 0x30, 0x35, 0x35,
	0x38, 0x31, 0x31, 0x31, 0x34, 0x30, 0x30, 0x33,
	0x00, 0x50, 0xf2, 0x4f, 0x65, 0x52, 0x00, 0x00,
	0x0a, 0x1e, 0x35, 0x33, 0x71, 0x85, 0x31, 0x4d,
	0x59, 0x12, 0x38, 0x48, 0x1c, 0x91, 0x53, 0x60,
	0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x75, 0x61, 0x57, 0xfb, 0x2c, 0x01, 0x00, 0x00,
	0x45, 0x53, 0x54, 0x00, 0x45, 0x44, 0x54, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x05, 0x00, 0x02, 0x04, 0x01, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xc4, 0xff, 0xff, 0xff,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static_assert(sizeof(s_default_eeprom) == 256);


/** Private device implementation **/
class eeprom::Impl
{
public:
	void init(machine *machine);
	void deinit();
	uint8_t read_byte(uint8_t command);
	void write_byte(uint8_t command, uint8_t value);
	uint16_t read_word(uint8_t command);
	void write_word(uint8_t command, uint16_t value);

private:
	std::optional<std::fstream> createDefault(std::filesystem::path eeprom_dir, uint8_t *buff);

	std::fstream m_fs;
	uint8_t m_eeprom[256];
};

uint8_t eeprom::Impl::read_byte(uint8_t command)
{
	return m_eeprom[command];
}

void eeprom::Impl::write_byte(uint8_t command, uint8_t value)
{
	m_eeprom[command] = value;
}

uint16_t eeprom::Impl::read_word(uint8_t command)
{
	return m_eeprom[command] | (((uint16_t)m_eeprom[command + 1]) << 8);
}

void eeprom::Impl::write_word(uint8_t command, uint16_t value)
{
	m_eeprom[command] = value & 0xFF;
	m_eeprom[command + 1] = value >> 8;
}

void eeprom::Impl::deinit()
{
	m_fs.seekg(0);
	m_fs.write((const char *)m_eeprom, 256);
	if (!m_fs.good()) {
		logger_en(error, "Failed to flush eeprom file to disk");
	}
}

void eeprom::Impl::init(machine *machine)
{
	uintmax_t size;
	std::optional<std::fstream> opt;
	std::filesystem::path eeprom_dir = combine_file_paths(emu_path::g_nxbx_dir, "eeprom.bin");
	if (opt = open_file(eeprom_dir, &size); !opt) {
		if (opt = createDefault(eeprom_dir, m_eeprom); !opt) {
			throw std::runtime_error(lv2str(highest, "Failed to create new eeprom file"));
		}
	}
	else {
		if (size == 256) {
			opt->read((char *)m_eeprom, 256);
			if (!opt->good()) {
				throw std::runtime_error(lv2str(highest, "Failed to copy eeprom file to memory"));
			}
		}
		else if (size == 0) {
			if (opt = createDefault(eeprom_dir, m_eeprom); !opt) {
				throw std::runtime_error(lv2str(highest, "Failed to overwrite eeprom file of size zero"));
			}
		}
		else {
			throw std::runtime_error(lv2str(highest, ("Unexpected eeprom file size (it was " + std::to_string(size) + " bytes vs 256 expected)").c_str()));
		}
	}
	m_fs = std::move(*opt);
}

std::optional<std::fstream> eeprom::Impl::createDefault(std::filesystem::path eeprom_dir, uint8_t *buff)
{
	if (auto opt = create_file(eeprom_dir); !opt) {
		logger_en(error, "Failed to create eeprom file");
		return std::nullopt;
	}
	else {
		opt->write((const char *)s_default_eeprom, 256);
		if (!opt->good()) {
			logger_en(error, "Failed to create default eeprom file");
			return std::nullopt;
		}
		std::memcpy(buff, s_default_eeprom, 256);
		return opt;
	}
}

/** Public interface implementation **/
void eeprom::init(machine *machine, log_module log_module)
{
	m_log_module = log_module;
	m_impl->init(machine);
}

void eeprom::deinit()
{
	if (m_impl) {
		m_impl->deinit();
	}
}

uint8_t eeprom::read_byte(uint8_t command)
{
	return m_impl->read_byte(command);
}

void eeprom::write_byte(uint8_t command, uint8_t value)
{
	m_impl->write_byte(command, value);
}

uint16_t eeprom::read_word(uint8_t command)
{
	return m_impl->read_word(command);
}

void eeprom::write_word(uint8_t command, uint16_t value)
{
	m_impl->write_word(command, value);
}

eeprom::eeprom() : m_impl{std::make_unique<eeprom::Impl>()} {}
eeprom::~eeprom() {}
