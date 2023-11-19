// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720


#include "files.hpp"
#include "logger.hpp"
#include <fstream>


bool
file_exists(std::filesystem::path path)
{
	try {
		return std::filesystem::exists(path);
	}
	catch (const std::filesystem::filesystem_error &e) {
		logger(log_lv::info, "Failed to check existence of path %s, the error was %s", path.string().c_str(), e.what());
	}
	catch (const std::bad_alloc &e) {
		logger(log_lv::info, "Failed to check existence of path %s, the error was %s", path.string().c_str(), e.what());
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
				logger(log_lv::info, "Failed to created directory %s", path.string().c_str());
				return false;
			}
		}

		return true;
	}
	catch (const std::filesystem::filesystem_error &e) {
		logger(log_lv::info, "Failed to created directory %s, the error was %s", path.string().c_str(), e.what());
	}
	catch (const std::bad_alloc &e) {
		logger(log_lv::info, "Failed to created directory %s, the error was %s", path.string().c_str(), e.what());
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
open_file(std::filesystem::path path)
{
	std::fstream fs(path, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
	return fs.is_open() ? std::make_optional<std::fstream>(std::move(fs)) : std::nullopt;
}
