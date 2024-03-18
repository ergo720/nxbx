// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720


#include "files.hpp"
#include "logger.hpp"
#include "util.hpp"
#include <fstream>
#if defined(_WIN64)
#include "Windows.h"
#undef min
#elif defined(__linux__)
#include <unistd.h>
#endif

#define MODULE_NAME file


bool
file_exists(std::pair<std::filesystem::path, std::string> path_to_check, std::filesystem::path &resolved_path)
{
	// path_to_check.first -> base device part, decided by host, path_to_check.second -> remaining variable part, decided by xbox

	try {
		resolved_path = path_to_check.first / path_to_check.second;
		bool exists = std::filesystem::exists(resolved_path);
		if (!exists) {
			// If it failed, the path might still exists, but the OS filesystem doesn't do case-insensitive comparisons (which the xbox always does)
			// E.g.: on Linux, the ext4 filesystem might trigger this case

			if (path_to_check.second.empty()) {
				return false;
			}

			// This starts from the device path, and checks each file name component of the path for a case-insensitive match with a host file in the current inspected directory
			size_t pos_to_check = 0, name_size = path_to_check.second.size();
			int64_t remaining_path_size = name_size;
			std::filesystem::path local_path(path_to_check.first);
			do {
				size_t pos = path_to_check.second.find_first_of('\\', pos_to_check);
				pos = std::min(pos, name_size);
				util::xbox_string_view xbox_name(util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(std::string_view(&path_to_check.second[pos_to_check], pos - pos_to_check)));
				for (const auto &directory_entry : std::filesystem::directory_iterator(local_path)) {
					std::string host_name(directory_entry.path().filename().string());
					util::xbox_string xbox_host_name(util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(host_name));
					if (xbox_name.compare(xbox_host_name) == 0) {
						local_path /= host_name;
						pos_to_check = pos + 1;
						remaining_path_size = name_size - pos_to_check;
						goto found_name;
					}
				}
				return false;

				// NOTE: labels belong to statements, so the final semicolon is required. This restriction is lifted in C++23
			found_name: ;
			} while (remaining_path_size > 0);

			resolved_path = local_path;
		}
		return true;
	}
	catch (const std::filesystem::filesystem_error &e) {
		loggerex1(info, "Failed to check existence of path %s, the error was %s", resolved_path.string().c_str(), e.what());
	}
	catch (const std::bad_alloc &e) {
		loggerex1(info, "Failed to check existence of path %s, the error was %s", resolved_path.string().c_str(), e.what());
	}

	return false;
}

bool
file_exists(std::pair<std::filesystem::path, std::string> path_to_check, std::filesystem::path &resolved_path, bool *is_directory)
{
	if (file_exists(path_to_check, resolved_path)) {
		try {
			*is_directory = std::filesystem::is_directory(resolved_path);
			return true;
		}
		catch (const std::filesystem::filesystem_error &e) {
			loggerex1(info, "Failed to determine the file type of path %s, the error was %s", resolved_path.string().c_str(), e.what());
		}
		catch (const std::bad_alloc &e) {
			loggerex1(info, "Failed to determine the file type of path %s, the error was %s", resolved_path.string().c_str(), e.what());
		}
	}

	return false;
}

bool
file_exists(std::filesystem::path path)
{
	try {
		return std::filesystem::exists(path);
	}
	catch (const std::filesystem::filesystem_error &e) {
		loggerex1(info, "Failed to determine the file type of path %s, the error was %s", path.string().c_str(), e.what());
	}
	catch (const std::bad_alloc &e) {
		loggerex1(info, "Failed to determine the file type of path %s, the error was %s", path.string().c_str(), e.what());
	}

	return false;
}

bool
create_directory(std::filesystem::path path)
{
	try {
		std::string path_no_slash = path.string();
		if ((path_no_slash[path_no_slash.size() - 1] == '/') || (path_no_slash[path_no_slash.size() - 1] == '\\')) {
			// NOTE: std::filesystem::create_directories returns false if path has a trailing slash, even if it successfully creates the directory
			path_no_slash = path_no_slash.substr(0, path_no_slash.size() - 1);
			path = path_no_slash;
		}
		bool exists = std::filesystem::exists(path);
		if (!exists) {
			exists = std::filesystem::create_directories(path);
			if (!exists) {
				loggerex1(info, "Failed to created directory %s", path.string().c_str());
				return false;
			}
		}

		return true;
	}
	catch (const std::filesystem::filesystem_error &e) {
		loggerex1(info, "Failed to created directory %s, the error was %s", path.string().c_str(), e.what());
	}
	catch (const std::bad_alloc &e) {
		loggerex1(info, "Failed to created directory %s, the error was %s", path.string().c_str(), e.what());
	}

	return false;
}

std::optional<std::fstream>
create_file(std::filesystem::path path)
{
	// NOTE: despite the ios_base flags used, this can still fail (e.g. file is read-only on the OS filesystem)
	std::fstream fs(path, std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
	return fs.is_open() ? std::make_optional<std::fstream>(std::move(fs)) : std::nullopt;
}

std::optional<std::fstream>
create_file(std::filesystem::path path, uint64_t initial_size)
{
	if (auto opt = create_file(path)) {
		if (initial_size) {
			try {
				std::filesystem::resize_file(path, initial_size);
			}
			catch (const std::filesystem::filesystem_error &e) {
				loggerex1(info, "Failed to set the initial file size of path %s, the error was %s", path.string().c_str(), e.what());
			}
			catch (const std::bad_alloc &e) {
				loggerex1(info, "Failed to set the initial file size of path %s, the error was %s", path.string().c_str(), e.what());
			}
		}
		return opt;
	}
	return std::nullopt;
}

std::optional<std::fstream>
open_file(std::filesystem::path path)
{
	std::fstream fs(path, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
	return fs.is_open() ? std::make_optional<std::fstream>(std::move(fs)) : std::nullopt;
}

std::optional<std::fstream>
open_file(std::filesystem::path path, std::uintmax_t *size)
{
	if (auto opt = open_file(path)) {
		try {
			*size = std::filesystem::file_size(path);
		}
		catch (const std::filesystem::filesystem_error &e) {
			*size = 0;
			loggerex1(info, "Failed to determine the file size of path %s, the error was %s", path.string().c_str(), e.what());
		}
		catch (const std::bad_alloc &e) {
			*size = 0;
			loggerex1(info, "Failed to determine the file size of path %s, the error was %s", path.string().c_str(), e.what());
		}
		return opt;
	}
	return std::nullopt;
}

void
xbox_to_host_separator(std::string &path)
{
#if defined(__linux__)
	// Note that on Linux, the slash is a valid character for file names, so std::filesystem::path::make_preferred won't change them
	std::replace(path.begin(), path.end(), '\\', '/');
#endif
}

namespace nxbx {
	std::string
	get_path()
	{
		// NOTE: Using argv[0] is unreliable, since it might or might not have the full path of the program

		std::unique_ptr<char[]> path_buff(new char[260]);

#if defined(_WIN64)
		DWORD ret, size = 260;
		while (true) {
			ret = GetModuleFileName(NULL, path_buff.get(), size - 1);
			if ((ret == 0) || (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
				size *= 2;
				path_buff = std::unique_ptr<char[]>(new char[size]);
				continue;
			}
			path_buff[ret] = '\0';
			return path_buff.get();
		}
#elif defined(__linux__)
		size_t size = 260;
		while (true) {
			ssize_t ret = readlink("/proc/self/exe", path_buff.get(), size - 1);
			if ((ret == -1) || (ret == size)) {
				size *= 2;
				path_buff = std::unique_ptr<char[]>(new char[size]);
				continue;
			}
			path_buff[ret] = '\0';
			return path_buff.get();
		}
#else
#error "Don't know how to retrieve the path of nxbx on this platform"
#endif
	}
}
