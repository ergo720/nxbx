// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "lib86cpu.h"

#define CONTIGUOUS_MEMORY_BASE 0x80000000
#define KERNEL_BASE 0x80010000


enum KERNEL_IO {
	KERNEL_IO_BASE = 0X200,
	DBG_STR = 0X200,
	SYS_TYPE,
	ABORT,
	BOOT_TIME_LOW,
	BOOT_TIME_HIGH,
	BOOT_TIME_MS,
	IO_START,
	IO_RETRY,
	IO_QUERY_STATUS,
	IO_QUERY_INFO,
	IO_CHECK_ENQUEUE,
	IO_SET_ID,
	XE_DVD_XBE_LENGTH,
	XE_DVD_XBE_ADDR,
	KERNEL_IO_END
};
constexpr inline size_t KERNEL_IO_SIZE = KERNEL_IO_END - KERNEL_IO_BASE;
inline std::string xbe_name;

uint32_t nboxkrnl_read_handler(addr_t addr, void *opaque);
void nboxkrnl_write_handler(addr_t addr, const uint32_t value, void *opaque);
