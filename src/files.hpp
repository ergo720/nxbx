// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <filesystem>
#include <optional>
#include <fstream>


bool create_directory(const std::filesystem::path path);
bool file_exists(const std::filesystem::path dev_path, const std::string remaining_name, std::filesystem::path &resolved_path);
bool file_exists(const std::filesystem::path dev_path, const std::string remaining_name, std::filesystem::path &resolved_path, bool *is_directory);
bool file_exists(const std::filesystem::path path);
std::optional<std::fstream> create_file(const std::filesystem::path path);
std::optional<std::fstream> create_file(const std::filesystem::path path, uint64_t initial_size);
std::optional<std::fstream> open_file(const std::filesystem::path path);
std::optional<std::fstream> open_file(const std::filesystem::path path, std::uintmax_t *size);
std::filesystem::path to_slash_separator(const std::filesystem::path path);
