// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#include "xbe.hpp"
#include "files.hpp"
#include "logger.hpp"
#include <cstring>


namespace xbe {
	constexpr char magic[] = { 'X', 'B', 'E', 'H' };


	bool
	validate(std::string_view arg_str)
	{
		if (auto opt = open_file(arg_str)) {
			// XBE: magic is 4 bytes at the very beginning of the file

			char buff[4];
			opt->seekg(0);
			opt->read(buff, 4);
			if (opt->good() && (std::memcmp(buff, magic, 4) == 0)) {
				logger("Detected xbe file");
				return true;
			}
		}

		return false;
	}
}
