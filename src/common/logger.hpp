// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdarg>
#include <stdexcept>
#include <string>
#include <atomic>
#include <array>
#include <algorithm>
#include <utility>
#include <unordered_map>

#define logger_en(lv, msg, ...) do { logger<log_lv::lv, log_module::MODULE_NAME, true>(msg __VA_OPT__(,) __VA_ARGS__); } while(0)
#define logger_mod_en(lv, mod, msg, ...) do { logger<log_lv::lv, log_module::mod, true>(msg __VA_OPT__(,) __VA_ARGS__); } while(0)
#define log_io_read() log_read<log_module::MODULE_NAME, false, 0>(m_regs_info, addr, value)
#define log_io_write() log_write<log_module::MODULE_NAME, false, 0>(m_regs_info, addr, value)
#define module_enabled() check_if_enabled<log_module::MODULE_NAME>()

#define NUM_OF_LOG_MODULES32 std::to_underlying(log_module::max) / 32 + 1


enum class log_lv : int32_t {
	lowest = -1,
	debug,
	info,
	warn,
	error,
	highest,
	max,
};

enum class log_module : int32_t {
	lowest = -1,
	nxbx,
	file,
	io,
	kernel,
	pit,
	pic,
	pci,
	cpu,
	cmos,
	vga,
	pbus,
	pcrtc,
	pfb,
	pfifo,
	pmc,
	pramdac,
	pramin,
	ptimer,
	pvga,
	pvideo,
	puser,
	pgraph,
	smbus,
	eeprom,
	smc,
	adm1032,
	conexant,
	usb0,
	max,
};

inline constexpr std::array module_to_str = {
	"NXBX -> ",
	"FILE -> ",
	"IO -> ",
	"KERNEL -> ",
	"PIT -> ",
	"PIC -> ",
	"PCI -> ",
	"CPU -> ",
	"CMOS -> ",
	"VGA -> ",
	"NV2A.PBUS -> ",
	"NV2A.PCRTC -> ",
	"NV2A.PFB -> ",
	"NV2A.PFIFO -> ",
	"NV2A.PMC -> ",
	"NV2A.PRAMDAC -> ",
	"NV2A.PRAMIN -> ",
	"NV2A.PTIMER -> ",
	"NV2A.PVGA -> ",
	"NV2A.PVIDEO -> ",
	"NV2A.PUSER -> ",
	"NV2A.PGRAPH -> ",
	"SMBUS -> ",
	"EEPROM -> ",
	"SMC -> ",
	"ADM -> ",
	"CONEXANT -> ",
	"USB0 -> "
};
static_assert(module_to_str.size() == (uint32_t)(log_module::max));

inline constexpr std::array lv_to_str = {
	"DBG:      ",
	"INFO:     ",
	"WARN:     ",
	"ERROR:    ",
	"CRITICAL: ",
};
static_assert(lv_to_str.size() == (uint32_t)(log_lv::max));

inline constexpr log_lv g_default_log_lv = log_lv::info;
inline constexpr uint32_t g_default_log_modules0 = 0;
inline std::atomic<log_lv> g_log_lv = g_default_log_lv;
inline std::atomic_uint32_t g_log_modules[NUM_OF_LOG_MODULES32] = {
	g_default_log_modules0
};


inline constexpr bool
is_log_lv_in_range(log_lv lv)
{
	return (std::to_underlying(lv) > std::to_underlying(log_lv::lowest)) &&
		(std::to_underlying(lv) < std::to_underlying(log_lv::max));
}

inline constexpr bool
is_log_module_in_range(log_module name)
{
	return (std::to_underlying(name) > std::to_underlying(log_module::lowest)) &&
		(std::to_underlying(name) < std::to_underlying(log_module::max));
}

inline bool
check_if_enabled(log_module name)
{
	if (is_log_module_in_range(name)) {
		return g_log_modules[(uint32_t)name / 32] & (1 << ((uint32_t)name % 32));
	}
	else {
		return 0;
	}
}

template<log_module name>
inline bool check_if_enabled()
{
	if constexpr (is_log_module_in_range(name)) {
		return g_log_modules[(uint32_t)name / 32] & (1 << ((uint32_t)name % 32));
	}
	else {
		throw std::logic_error("Out of range log_module used");
	}
}

inline void
logger(const char *msg, std::va_list vlist)
{
	std::string str(msg);
	str += '\n';
	std::vprintf(str.c_str(), vlist);
}

inline void
logger(const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger(msg, args);
	va_end(args);
}

template<log_lv lv, log_module name, bool check_if>
inline void logger(const char *msg, std::va_list vlist)
{
	if constexpr (is_log_lv_in_range(lv) && is_log_module_in_range(name)) {
		if constexpr (check_if) {
			if ((lv < g_log_lv) || !check_if_enabled<name>()) {
				return;
			}
		}
		std::string str(lv_to_str[std::to_underlying(lv)]);
		str += module_to_str[std::to_underlying(name)];
		str += msg;
		str += '\n';
		std::vprintf(str.c_str(), vlist);
	}
	else {
		throw std::logic_error("Out of range log_lv and/or log_module used");
	}
}

template<log_lv lv, log_module name, bool check_if>
inline void logger(const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger<lv, name, check_if>(msg, args);
	va_end(args);
}

template<log_module name, bool check_if>
inline void logger(log_lv lv, const char *msg, std::va_list vlist)
{
	if constexpr (is_log_module_in_range(name)) {
		if (is_log_lv_in_range(lv)) {
			if constexpr (check_if) {
				if ((lv < g_log_lv) || !check_if_enabled<name>()) {
					return;
				}
			}
			std::string str(lv_to_str[std::to_underlying(lv)]);
			str += module_to_str[std::to_underlying(name)];
			str += msg;
			str += '\n';
			std::vprintf(str.c_str(), vlist);
		}
		else {
			logger("Out of range log_lv used");
		}
	}
	else {
		throw std::logic_error("Out of range log_module used");
	}
}

template<log_module name, bool check_if>
inline void logger(log_lv lv, const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger<name, check_if>(lv, msg, args);
	va_end(args);
}

template<log_lv lv, bool check_if>
inline void logger(log_module name, const char *msg, std::va_list vlist)
{
	if constexpr (is_log_lv_in_range(lv)) {
		if (is_log_module_in_range(name)) {
			if constexpr (check_if) {
				if ((lv < g_log_lv) || !check_if_enabled(name)) {
					return;
				}
			}
			std::string str(lv_to_str[std::to_underlying(lv)]);
			str += module_to_str[std::to_underlying(name)];
			str += msg;
			str += '\n';
			std::vprintf(str.c_str(), vlist);
		}
		else {
			logger("Out of range log_module used");
		}
	}
	else {
		throw std::logic_error("Out of range log_lv used");
	}
}

template<log_lv lv, bool check_if>
inline void logger(log_module name, const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger<lv, check_if>(name, msg, args);
	va_end(args);
}

template<log_module name, bool check_if, uint32_t align_mask>
void log_write(const std::unordered_map<uint32_t, const std::string> &regs_info, uint32_t addr, uint32_t value)
{
	const auto it = regs_info.find(addr & ~align_mask);
	logger<log_lv::debug, name, check_if>("Write at %s (0x%08X) of value 0x%08X", it != regs_info.end() ? it->second.c_str() : "UNKNOWN", addr, value);
}
template<log_module name, bool check_if, uint32_t align_mask>
void log_read(const std::unordered_map<uint32_t, const std::string> &regs_info, uint32_t addr, uint32_t value)
{
	const auto it = regs_info.find(addr & ~align_mask);
	logger<log_lv::debug, name, check_if>("Read at %s (0x%08X) of value 0x%08X", it != regs_info.end() ? it->second.c_str() : "UNKNOWN", addr, value);
}
