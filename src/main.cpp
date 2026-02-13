// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#include "files.hpp"
#include "nxbx.hpp"
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <QApplication>


static void
print_help()
{
	static const char *help =
		"usage: nxbx [options]\n\
options:\n\
-input <path>   Path to the XBE (xbox executable) or XISO (xbox disk image) to run\n\
-keys <path>    Path of xbox keys.bin file\n\
-kernel <path>  Path to nboxkrnl (xbox kernel) to run\n\
-disas <num>    Specify assembly syntax (default is Intel)\n\
-machine <name> Specify the console type to emulate (default is xbox)\n\
-sync_hdd <num> Synchronize hard disk partition metadata with partition folder\n\
-debug          Start with debugger\n\
-help           Print this message";

	logger("%s", help);
}

static std::optional<int>
parse_cmd_line_opt(const QStringList &args, init_info_t &init_info)
{
	const auto print_unk_opt = [](QStringList::ConstIterator it) {
		logger("Unknown option %s", qPrintable(*it));
		print_help();
		return 1;
		};

	const auto check_missing_arg = [&args](QStringList::ConstIterator &it) {
		it = std::next(it);
		if ((it == args.end()) || (*it->data() == '-')) {
			logger("Missing argument for option \"%s\"", qPrintable(it == args.end() ? *std::prev(it) : *it));
			return 1;
		}
		return 0;
		};

	if (args.size() < 3) {
		print_help();
		return 0;
	}

	for (auto it = std::next(args.begin()); it != args.end(); ++it) {
		try {
			if (it->front() != '-') { // all options start with a dash
				return print_unk_opt(it);
			}
			else {
				if (*it == QStringLiteral("-input")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					if (!nxbx::validate_input_file(init_info, qPrintable(*it))) {
						return 1;
					}
					init_info.m_input_path = qPrintable(*it);
				}
				else if (*it == QStringLiteral("-kernel")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					init_info.m_kernel_path = qPrintable(*it);
				}
				else if (*it == QStringLiteral("-disas")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					switch (init_info.m_syntax = static_cast<disas_syntax>(std::stoul(std::string(qPrintable(*it)), nullptr, 0)))
					{
					case disas_syntax::att:
					case disas_syntax::masm:
					case disas_syntax::intel:
						break;

					default:
						logger("Unknown syntax specified by option \"-disas\"");
						return 1;
					}
				}
				else if (*it == QStringLiteral("-machine")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					std::string console = qPrintable(*it);
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
							logger("Unknown console type specified by option \"-machine\"");
							return 1;
						}
					}
				}
				else if (*it == QStringLiteral("-debug")) {
					init_info.m_use_dbg = 1;
				}
				else if (*it == QStringLiteral("-help")) {
					print_help();
					return 0;
				}
				else if (*it == QStringLiteral("-keys")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					init_info.m_keys_path = qPrintable(*it);
				}
				else if (*it == QStringLiteral("-sync_hdd")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					init_info.m_sync_part = std::stoul(qPrintable(*it));
					if (init_info.m_sync_part > 5) {
						logger("Invalid partition number %u specified by option \"-sync_hdd\" (must be in the range [0-5])", init_info.m_sync_part);
						return 1;
					}
				}
				else {
					return print_unk_opt(it);
				}
			}
		}
		/* handle possible exceptions thrown by std::stoul */
		catch (const std::exception &e) {
			logger("Failed to parse \"%s\" option. The error was: %s", qPrintable(*std::prev(it)), e.what());
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

	init_info.m_nxbx_path = qPrintable(QCoreApplication::applicationDirPath());
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

	return std::nullopt;
}

int
main(int argc, char **argv)
{
	std::locale::global(std::locale(""));

	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

	QApplication app(argc, argv);

	init_info_t init_info;
	init_info.m_keys_path = "";
	init_info.m_syntax = disas_syntax::intel;
	init_info.m_console_type = console_t::xbox;
	init_info.m_use_dbg = 0;
	init_info.m_sync_part = -1; // -1=don't sync, 0=sync all partitions, [1-7]=sync that partition

	/* parameter parsing */
	if (const auto &opt = parse_cmd_line_opt(app.arguments(), init_info); opt) {
		return *opt;
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
