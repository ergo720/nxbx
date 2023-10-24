// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <string>
#include "logger.hpp"


enum class disas_syntax : uint32_t {
	att,
	masm,
	intel
};

inline std::string xbe_path;
