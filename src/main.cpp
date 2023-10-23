// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "nxbx.hpp"
#include "hw/cpu.hpp"
#include <cstring>
#include <cstdio>


static void
print_help()
{
	static const char *help =
		"usage: [options] <path of the binary to run (if required)>\n\
options: \n\
-s <num>   Specify assembly syntax (default is AT&T)\n\
-d         Start with debugger\n\
-h         Print this message";

	logger("%s", help);
}

int
main(int argc, char **argv)
{
	std::string executable;
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

	if (cpu_init(executable, syntax, use_dbg) == false) {
		cpu_cleanup();
		return 1;
	}

	cpu_start();

	return 0;
}
