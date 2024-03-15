// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "eeprom.hpp"
#include <cstdint>


// This is bunnie's eeprom, except that it stores the encrypted settings unencrypted, because nboxkrnl cannot decrypt them yet
constexpr uint8_t default_eeprom[] = {
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


bool
gen_eeprom(std::fstream fs)
{
	fs.seekg(0, fs.beg);
	fs.write((const char *)default_eeprom, sizeof(default_eeprom));
	if (fs.rdstate() != std::ios_base::goodbit) {
		fs.clear();
		return false;
	}

	return true;
}
