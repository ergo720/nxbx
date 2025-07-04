// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "files.hpp"
#include "nxbx.hpp"
#include <cstring>
#include <cstdio>
#include <filesystem>


static void
print_help()
{
	static const char *help =
		"usage: nxbx [options]\n\
options:\n\
-i <path>       Path to the XBE (xbox executable) or XISO (xbox disk image) to run\n\
-keys <path>    Path of xbox keys.bin file\n\
-k <path>       Path to nboxkrnl (xbox kernel) to run\n\
-s <num>        Specify assembly syntax (default is Intel)\n\
-c <name>       Specify the console type to emulate (default is xbox)\n\
-sync_hdd <num> Synchronize hard disk partition metadata with partition folder\n\
-d              Start with debugger\n\
-h              Print this message";

	logger("%s", help);
}

int
main(int argc, char **argv)
{
	init_info_t init_info;
	init_info.m_keys_path = "";
	init_info.m_syntax = disas_syntax::intel;
	init_info.m_console_type = console_t::xbox;
	init_info.m_use_dbg = 0;
	init_info.m_sync_part = -1; // -1=don't sync, 0=sync all partitions, [1-7]=sync that partition
	char option = ' ';

	const auto print_unk_opt = [](std::string_view arg_str) {
		logger("Unknown option %s", arg_str.data());
		print_help();
		return 0;
		};

	const auto check_missing_arg = [argc, argv](int &idx, const char *opt) {
		if (++idx == argc || argv[idx][0] == '-') {
			logger("Missing argument for option \"%s\"", opt);
			return true;
		}
		return false;
		};

	/* parameter parsing */
	if (argc < 3) {
		print_help();
		return 0;
	}

	for (int idx = 1; idx < argc; idx++) {
		try {
			option = ' ';
			std::string arg_str(argv[idx]);
			if (arg_str.front() == '-') {
				if (arg_str.size() == 2) {

					// Process single-letter options
					switch (option = arg_str.at(1))
					{
					case 'i':
						if (check_missing_arg(idx, "i")) {
							return 0;
						}
						if (!nxbx::validate_input_file(init_info, argv[idx])) {
							return 1;
						}
						init_info.m_input_path = argv[idx];
						break;

					case 'k':
						if (check_missing_arg(idx, "k")) {
							return 0;
						}
						init_info.m_kernel_path = argv[idx];
						break;

					case 's':
						if (check_missing_arg(idx, "s")) {
							return 0;
						}
						switch (init_info.m_syntax = static_cast<disas_syntax>(std::stoul(std::string(argv[idx]), nullptr, 0)))
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

					case 'c': {
						if (check_missing_arg(idx, "c")) {
							return 0;
						}
						std::string console = argv[idx];
						if (console == nxbx::console_to_string(console_t::xbox)) {
							init_info.m_console_type = console_t::xbox;
						}
						else if (console == nxbx::console_to_string(console_t::chihiro)) {
							init_info.m_console_type = console_t::chihiro;
						}
						else if (console == nxbx::console_to_string(console_t::devkit)) {
							init_info.m_console_type = console_t::devkit;
						}
						else {
							switch (init_info.m_console_type = static_cast<console_t>(std::stoul(console, nullptr, 0)))
							{
							case console_t::xbox:
							case console_t::chihiro:
							case console_t::devkit:
								break;

							default:
								logger("Unknown console type specified by option \"%c\"", option);
								return 0;
							}
						}
					}
					break;

					case 'd':
						init_info.m_use_dbg = 1;
						break;

					case 'h':
						print_help();
						return 0;

					default:
						return print_unk_opt(arg_str);
					}
				}
				else {
					// Process multi-letter options
					if (arg_str.compare("-keys") == 0) {
						if (check_missing_arg(idx, arg_str.c_str())) {
							return 0;
						}
						init_info.m_keys_path = argv[idx];
					}
					else if (arg_str.compare("-sync_hdd") == 0) {
						if (check_missing_arg(idx, arg_str.c_str())) {
							return 0;
						}
						init_info.m_sync_part = std::stoul(argv[idx]);
						if (init_info.m_sync_part > 5) {
							logger("Invalid partition number %u specified by option -sync_hdd (must be in the range [0-5])", init_info.m_sync_part);
							return 0;
						}
					}
					else {
						return print_unk_opt(arg_str);
					}
				}
			}
			else {
				return print_unk_opt(arg_str);
			}
		}
		/* handle possible exceptions thrown by std::stoul */
		catch (const std::exception &e) {
			logger("Failed to parse \"%c\" option. The error was: %s", option, e.what());
			return 1;
		}
	}

	if (init_info.m_input_path.empty()) {
		logger("Input file is required");
		return 1;
	}

	// FIXME: remove this when the chihiro and devkit console types are supported
	if ((init_info.m_console_type == console_t::chihiro) || (init_info.m_console_type == console_t::devkit)) {
		logger("The %s console type is currently not supported", nxbx::console_to_string(init_info.m_console_type).data());
		return 1;
	}

	init_info.m_nxbx_path = nxbx::get_path();
	if (init_info.m_kernel_path.empty()) {
		// Attempt to find nboxkrnl in the current directory of nxbx
		std::filesystem::path curr_dir = init_info.m_nxbx_path;
		curr_dir = curr_dir.remove_filename();
		curr_dir /= "nboxkrnl.exe";
		std::error_code ec;
		bool exists = std::filesystem::exists(curr_dir, ec);
		if (ec || !exists) {
			logger("Unable to find \"nboxkrnl.exe\" in the current working directory");
			return 1;
		}
		init_info.m_kernel_path = curr_dir.string();
	}

	if (nxbx::init_settings(init_info) == false) {
		return 1;
	}

	if (nxbx::init_console(init_info) == false) {
		return 1;
	}

	nxbx::start();
	nxbx::exit();

	return 0;
}
