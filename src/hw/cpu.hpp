// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "../nxbx.hpp"
#include <string>


bool cpu_init(const std::string &executable, disas_syntax syntax, uint32_t use_dbg);
void cpu_start();
void cpu_cleanup();
