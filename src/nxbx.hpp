// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>
#include "logger.hpp"


enum class disas_syntax : uint32_t {
	att,
	masm,
	intel
};

class nxbx_exp_abort : public std::runtime_error
{
public:
	explicit nxbx_exp_abort(const std::string &msg) : runtime_error(msg.c_str()) { extra_info = msg.empty() ? false : true; }
	explicit nxbx_exp_abort(const char *msg) : runtime_error(msg) { extra_info = std::string_view(msg).empty() ? false : true; }
	bool has_extra_info() { return extra_info; }

private:
	bool extra_info;
};

void nxbx_fatal(const char *msg, ...);
