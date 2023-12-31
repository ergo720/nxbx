// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "clock.hpp"
#include "util.hpp"
#ifdef __linux__
#include <sys/time.h>
#elif _WIN64
#include "Windows.h"
#undef max
#endif


static uint64_t last_time;
static constexpr uint64_t xbox_acpi_freq = 3375000; // 3.375 MHz

#ifdef __linux__
void
timer_init()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	last_time = static_cast<uint64_t>(tv.tv_sec) * static_cast<uint64_t>(ticks_per_second) + static_cast<uint64_t>(tv.tv_usec);
}
#elif _WIN64
static uint64_t host_freq;

void
timer_init()
{
	LARGE_INTEGER freq, now;
	QueryPerformanceFrequency(&freq);
	host_freq = freq.QuadPart;
	QueryPerformanceCounter(&now);
	last_time = now.QuadPart;
}
#endif

uint64_t
get_now()
{
#ifdef __linux__
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t curr_time = static_cast<uint64_t>(tv.tv_sec) * static_cast<uint64_t>(ticks_per_second) + static_cast<uint64_t>(tv.tv_usec);
	return curr_time - last_time;
#elif _WIN64
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	uint64_t elapsed_us = static_cast<uint64_t>(now.QuadPart) - last_time;
	return muldiv128_(elapsed_us, ticks_per_second, host_freq);
#else
#error "don't know how to implement the get_now function on this OS"
#endif
}

uint64_t
get_acpi_now()
{
#ifdef _WIN64
	// NOTE: inlined get_now() to avoid having to use muldiv128_ twice
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	uint64_t elapsed_us = static_cast<uint64_t>(now.QuadPart) - last_time;
	return muldiv128_(elapsed_us, host_freq, xbox_acpi_freq);
#else
	return muldiv128_(get_now(), xbox_acpi_freq, ticks_per_second);
#endif
}
