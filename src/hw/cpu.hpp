// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "lib86cpu.h"
#include "../nxbx.hpp"
#include <string>


inline cpu_t *g_cpu = nullptr;

void cpu_init(const std::string &kernel, disas_syntax syntax, uint32_t use_dbg);
void cpu_start();
void cpu_cleanup();
