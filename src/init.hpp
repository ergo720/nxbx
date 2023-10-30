// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "nxbx.hpp"


using hw_reset_f = void(*)();

void reset_system();
void add_reset_func(unsigned idx, hw_reset_f reset_f);
void start_system(std::string kernel, disas_syntax syntax, uint32_t use_dbg);
