// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "nxbx.hpp"
#include "hw/cpu.hpp"
#include <cstring>
#include <cstdio>
#include <filesystem>


static void
print_help()
{
	static const char *help =
		"usage: [options] <path to the XBE (xbox executable) to run>\n\
options: \n\
-k <path>  Path to nboxkrnl (xbox kernel) to run\n\
-s <num>   Specify assembly syntax (default is AT&T)\n\
-d         Start with debugger\n\
-h         Print this message";

	logger("%s", help);
}

int
main(int argc, char **argv)
{
	std::string executable, kernel;
	disas_syntax syntax = disas_syntax::att;
	uint32_t use_dbg = 0;
	char option = ' ';

	/* parameter parsing */
	if (argc < 2) {
		print_help();
		return 0;
	}

	for (int idx = 1; idx < argc; idx++) {
		try {
			option = ' ';
			std::string arg_str(argv[idx]);
			if (arg_str.size() == 2 && arg_str.front() == '-') {
				switch (option = arg_str.at(1))
				{
				case 'k':
					if (++idx == argc || argv[idx][0] == '-') {
						logger("Missing argument for option \"%c\"", option);
						return 0;
					}
					kernel = argv[idx];
					break;

				case 's':
					if (++idx == argc || argv[idx][0] == '-') {
						logger("Missing argument for option \"s\"");
						return 0;
					}
					switch (syntax = static_cast<disas_syntax>(std::stoul(std::string(argv[idx]), nullptr, 0)))
					{
					case disas_syntax::att:
					case disas_syntax::masm:
					case disas_syntax::intel:
						break;

					default:
						logger("Unknown syntax specified by option \"%c\"", option);
						return 0;
					}
					break;

				case 'd':
					use_dbg = 1;
					break;

				case 'h':
					print_help();
					return 0;

				default:
					logger("Unknown option %s", arg_str.c_str());
					print_help();
					return 0;
				}
			}
			else if ((idx + 1) == argc) {
				executable = std::move(arg_str);
				break;
			}
			else {
				logger("Unknown option %s", arg_str.c_str());
				print_help();
				return 0;
			}
		}
		/* handle possible exceptions thrown by std::stoul */
		catch (const std::exception &e) {
			logger("Failed to parse \"%c\" option. The error was: %s", option, e.what());
			return 1;
		}
	}

	if (executable.empty()) {
		logger("Input file is required");
		return 1;
	}

	if (kernel.empty()) {
		// Attempt to find nboxkrnl in the current directory of nxbx
		std::filesystem::path curr_dir = argv[0];
		curr_dir = curr_dir.remove_filename();
		curr_dir /= "nboxkrnl.exe";
		std::error_code ec;
		bool exists = std::filesystem::exists(curr_dir, ec);
		if (ec || !exists) {
			logger("Unable to find \"nboxkrnl.exe\" in the current working directory");
			return 1;
		}
		kernel = curr_dir.string();
	}

	if (cpu_init(kernel, syntax, use_dbg) == false) {
		cpu_cleanup();
		return 1;
	}

	cpu_start();

	return 0;
}
