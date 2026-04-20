// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#include <QApplication>
#include <QtGui/QFileOpenEvent>

#include "files.hpp"
#include "isettings.hpp"
#include "console.hpp"
#include "qthost.hpp"
#include "main_window.hpp"
#include "paths.hpp"
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <mutex>

#define MODULE_NAME nxbx


static bool s_nogui_mode = false;

static std::fstream s_qt_log_file;
static std::mutex s_qt_log_mtx;

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
-no_gui         Start with no gui\n\
-debug          Start with debugger\n\
-help           Print this message";

	logger("%s", help);
}

static std::optional<int>
parse_cmd_line_opt(const QStringList &args, init_info_t &init_info)
{
	const auto print_unk_opt = [](QStringList::ConstIterator it) {
		log_init_failure("Unknown option %s", qPrintable(*it));
		print_help();
		return 1;
		};

	const auto check_missing_arg = [&args](QStringList::ConstIterator &it) {
		it = std::next(it);
		if ((it == args.end()) || (*it->data() == '-')) {
			log_init_failure("Missing argument for option \"%s\"", qPrintable(it == args.end() ? *std::prev(it) : *it));
			return 1;
		}
		return 0;
		};

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
					if (const auto exp = Host::validate_input_file(qPrintable(*it)); !exp) {
						return 1;
					}
					else {
						init_info.input_type = exp.value();
						init_info.input_path = to_slash_separator(qPrintable(*it)).string();
					}
				}
				else if (*it == QStringLiteral("-kernel")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					init_info.kernel_path = to_slash_separator(qPrintable(*it)).string();
				}
				else if (*it == QStringLiteral("-disas")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					switch (init_info.syntax = static_cast<disas_syntax>(std::stoul(std::string(qPrintable(*it)), nullptr, 0)))
					{
					case disas_syntax::att:
					case disas_syntax::masm:
					case disas_syntax::intel:
						break;

					default:
						log_init_failure("Unknown syntax specified by option \"-disas\"");
						return 1;
					}
				}
				else if (*it == QStringLiteral("-machine")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					std::string console = qPrintable(*it);
					if (console == console::to_string(console_t::xbox)) {
						init_info.console_type = console_t::xbox;
					}
					else if (console == console::to_string(console_t::chihiro)) {
						init_info.console_type = console_t::chihiro;
					}
					else if (console == console::to_string(console_t::devkit)) {
						init_info.console_type = console_t::devkit;
					}
					else {
						switch (init_info.console_type = static_cast<console_t>(std::stoul(console, nullptr, 0)))
						{
						case console_t::xbox:
						case console_t::chihiro:
						case console_t::devkit:
							break;

						default:
							log_init_failure("Unknown console type specified by option \"-machine\"");
							return 1;
						}
					}
				}
				else if (*it == QStringLiteral("-no_gui")) {
					s_nogui_mode = true;
				}
				else if (*it == QStringLiteral("-debug")) {
					init_info.use_dbg = 1;
				}
				else if (*it == QStringLiteral("-help")) {
					print_help();
					return 0;
				}
				else if (*it == QStringLiteral("-keys")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					init_info.keys_path = to_slash_separator(qPrintable(*it)).string();
				}
				else if (*it == QStringLiteral("-sync_hdd")) {
					if (check_missing_arg(it)) {
						return 1;
					}
					init_info.sync_part = std::stoul(qPrintable(*it));
					if (init_info.sync_part > 5) {
						log_init_failure("Invalid partition number %u specified by option \"-sync_hdd\" (must be in the range [0-5])", init_info.sync_part);
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
			log_init_failure("Failed to parse \"%s\" option. The error was: %s", qPrintable(*std::prev(it)), e.what());
			return 1;
		}
	}

	if (s_nogui_mode && init_info.input_path.empty()) {
		log_init_failure("Input file is required");
		return 1;
	}

	// FIXME: remove this when the chihiro and devkit console types are supported
	if ((init_info.console_type == console_t::chihiro) || (init_info.console_type == console_t::devkit)) {
		log_init_failure("The %s console type is currently not supported", console::to_string(init_info.console_type).data());
		return 1;
	}

	init_info.nxbx_dir = qPrintable(QCoreApplication::applicationDirPath()); // NOTE: the path returned by Qt already uses slashes

	return std::nullopt;
}

bool Host::InNoGUIMode()
{
	return s_nogui_mode;
}

class NxbxMainApplication : public QApplication
{
public:
	using QApplication::QApplication;

	bool event(QEvent* event) override
	{
		if (event->type() == QEvent::FileOpen)
		{
			QFileOpenEvent* open = static_cast<QFileOpenEvent*>(event);
			const QUrl url = open->url();
			if (url.isLocalFile()) {
				return g_main_window->startFile(url.toLocalFile());
			}
			else {
				return false; // No URL schemas currently supported
			}
		}
		return QApplication::event(event);
	}
};

static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	std::unique_lock<std::mutex> lock{s_qt_log_mtx};
	QByteArray local_msg = msg.toLocal8Bit();

	switch (type)
	{
	case QtDebugMsg:
		s_qt_log_file << "Debug: ";
		break;

	case QtInfoMsg:
		s_qt_log_file << "Info: ";
		break;

	case QtWarningMsg:
		s_qt_log_file << "Warning: ";
		break;

	case QtCriticalMsg:
		s_qt_log_file << "Critical: ";
		break;

	case QtFatalMsg:
		s_qt_log_file << "Fatal: ";
		break;

	default:
		s_qt_log_file << "Unknown: ";
	}

	// It seems that Qt sets the pointers of the context in debug builds only, otherwise they are nullptr
	if (context.file && context.function) {
		s_qt_log_file << local_msg.constData() << " (" << context.file << ':' << context.line << ", " << context.function << ")\n";
	}
	else {
		s_qt_log_file << local_msg.constData() << '\n';
	}

	// Abort if the error was fatal
	if (type == QtFatalMsg) {
		if (g_console) {
			g_console->exit();
		}
		else {
			lock.unlock();
			std::abort();
		}
	}
}

int
main(int argc, char **argv)
{
	// Logging the cpu module causes a ton of overhead on a Windows Console Host, so switch to full buffering and increase the buffer size
	// Here, we are using the size that POSIX fstat usually retuns
	[[maybe_unused]] int setvbuf_ret = std::setvbuf(stdout, nullptr, _IOFBF, 65536);
	assert(setvbuf_ret == 0);

	// Timestamps in some locales showed up wrong on Windows.
	// Qt already applies the user locale on Unix-like systems.
#ifdef _WIN32
	std::locale::global(std::locale(""));
#endif

	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

	NxbxMainApplication app(argc, argv);

	init_info_t init_info;
	init_info.syntax = disas_syntax::intel;
	init_info.console_type = console_t::xbox;
	init_info.input_type = input_t::invalid;
	init_info.use_dbg = 0;
	init_info.sync_part = -1; // -1=don't sync, 0=sync all partitions, [1-7]=sync that partition

	// Parameter parsing
	if (const auto &opt = parse_cmd_line_opt(app.arguments(), init_info); opt) {
		return *opt;
	}

	// Setup our global paths
	if (emu_path::setup(init_info) == false) {
		return 1;
	}

	// Install Qt message handler
	s_qt_log_file = std::fstream(emu_path::g_qt_log_path, std::ios_base::out | std::ios_base::trunc);
	qInstallMessageHandler(qtMessageHandler);

	// Setup ini configuration file
	if (init_settings() == false) {
		return 1;
	}

	// Find the kernel, in the case its path wasn't passed from the command line
	init_info.kernel_path = Host::SetupKernelPath(init_info.kernel_path);
	if (init_info.kernel_path.empty()) {
		log_init_failure("Unable to find \"nboxkrnl.exe\" file");
		return 1;
	}

	// Set theme before creating any windows.
	QtHost::UpdateApplicationTheme();

	boot_params params;
	params.console_type = init_info.console_type;
	params.syntax = init_info.syntax;
	params.use_dbg = init_info.use_dbg;

	g_console = new console(params);
	if (g_console->get_state() == console_state::shut_down) {
		delete g_console;
		return 1;
	}

	// Create all window objects
	g_main_window = new MainWindow();
	g_main_window->initialize();

	if (s_nogui_mode) {
		// Start the emulation in the cpu thread
		g_console->start();
	}
	else {
		g_main_window->show();
		g_main_window->raise();
		g_main_window->activateWindow();

		// Start the emulation if we have an input file
		if (!init_info.input_path.empty()) {
			g_console->start();
		}
	}

	// This doesn't return until we exit.
	int result = app.exec();

	// Shutting down.
	if (g_console) {
		g_console->exit(true);
	}
	if (g_main_window) {
		g_main_window->close();
	}
	delete g_main_window;
	delete g_console;

	save_settings();

	return result;
}
