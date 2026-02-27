// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#include "clock.hpp"
#include "util.hpp"
#ifdef __linux__
#include <sys/time.h>
#elif _WIN64
#include "Windows.h"
#undef max
#endif


namespace timer {
	static uint64_t s_last_time;
	static constexpr uint64_t s_xbox_acpi_freq = 3375000; // 3.375 MHz

#ifdef __linux__
	void
	init()
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		s_last_time = util::muldiv128(static_cast<uint64_t>(tv.tv_sec), g_ticks_per_second, 1ULL) + static_cast<uint64_t>(tv.tv_usec);
	}
#elif _WIN64
	static uint64_t s_host_freq;

	void
	init()
	{
		LARGE_INTEGER freq, now;
		QueryPerformanceFrequency(&freq);
		s_host_freq = freq.QuadPart;
		QueryPerformanceCounter(&now);
		s_last_time = now.QuadPart;
	}
#endif

	uint64_t
	get_now()
	{
#ifdef __linux__
		timeval tv;
		gettimeofday(&tv, NULL);
		uint64_t curr_time = util::muldiv128(static_cast<uint64_t>(tv.tv_sec), g_ticks_per_second, 1ULL) + static_cast<uint64_t>(tv.tv_usec);
		return curr_time - s_last_time;
#elif _WIN64
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		uint64_t elapsed_us = static_cast<uint64_t>(now.QuadPart) - s_last_time;
		return util::muldiv128(elapsed_us, g_ticks_per_second, s_host_freq);
#else
#error "don't know how to implement the get_now function on this OS"
#endif
	}

	uint64_t
	get_dev_now(uint64_t dev_freq)
	{
#ifdef _WIN64
		// NOTE: inlined get_now() to avoid having to use muldiv128_ twice
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		uint64_t elapsed_us = static_cast<uint64_t>(now.QuadPart) - s_last_time;
		return util::muldiv128(elapsed_us, dev_freq, s_host_freq);
#else
		return util::muldiv128(get_now(), dev_freq, g_ticks_per_second);
#endif
	}

	uint64_t
	get_acpi_now()
	{
		return get_dev_now(s_xbox_acpi_freq);
	}
}
