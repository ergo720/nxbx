// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <filesystem>
#include <optional>
#include <fstream>


bool create_directory(std::filesystem::path path);
bool file_exists(std::filesystem::path path);
bool file_exists(std::filesystem::path path, bool *is_directory);
std::optional<std::fstream> create_file(std::filesystem::path path);
std::optional<std::fstream> open_file(std::filesystem::path path);
std::string get_nxbx_path();
