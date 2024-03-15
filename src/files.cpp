// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720


#include "files.hpp"
#include "logger.hpp"
#include "util.hpp"
#include <fstream>
#if defined(_WIN64)
#include "Windows.h"
#elif defined(__linux__)
#include <unistd.h>
#endif

#define MODULE_NAME file


bool
file_exists(std::filesystem::path path)
{
	try {
		// TODO: this can probably spuriously fail if the OS filesystem doesn't do case-insensitive comparisons
		return std::filesystem::exists(path);
	}
	catch (const std::filesystem::filesystem_error &e) {
		loggerex1(info, "Failed to check existence of path %s, the error was %s", path.string().c_str(), e.what());
	}
	catch (const std::bad_alloc &e) {
		loggerex1(info, "Failed to check existence of path %s, the error was %s", path.string().c_str(), e.what());
	}

	return false;
}

bool
file_exists(std::filesystem::path path, bool *is_directory)
{
	if (file_exists(path)) {
		try {
			*is_directory = std::filesystem::is_directory(path);
			return true;
		}
		catch (const std::filesystem::filesystem_error &e) {
			loggerex1(info, "Failed to determine the file type of path %s, the error was %s", path.string().c_str(), e.what());
		}
		catch (const std::bad_alloc &e) {
			loggerex1(info, "Failed to determine the file type of path %s, the error was %s", path.string().c_str(), e.what());
		}
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
