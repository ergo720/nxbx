// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "nxbx.hpp"


using hw_reset_f = void(*)();

class init_info_t {
public:
	std::string m_kernel;
	std::string m_nxbx_path;
	std::string m_xbe_path;
	disas_syntax m_syntax;
	uint32_t m_use_dbg;
};


void reset_system();
void add_reset_func(hw_reset_f reset_f);
void start_system(init_info_t init_info);
