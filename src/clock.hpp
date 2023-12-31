// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>


inline constexpr uint64_t ticks_per_second = 1000000;

void timer_init();
uint64_t get_now();
uint64_t get_acpi_now();
