// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "util.hpp"

#define CONTIGUOUS_MEMORY_BASE 0x80000000
#define KERNEL_BASE 0x80010000


namespace kernel {
	enum IO_PORTS {
		IO_BASE = 0X200,
		DBG_STR = 0X200,
		SYS_TYPE,
		ABORT,
		CLOCK_INCREMENT_LOW,
		CLOCK_INCREMENT_HIGH,
		BOOT_TIME_MS,
		IO_START,
		IO_RETRY,
		IO_QUERY,
		UNUSED1,
		IO_CHECK_ENQUEUE,
		UNUSED2,
		UNUSED3,
		XE_DVD_XBE_LENGTH,
		XE_DVD_XBE_ADDR,
		ACPI_TIME_LOW,
		ACPI_TIME_HIGH,
		IO_END
	};
	constexpr inline size_t IO_SIZE = IO_END - IO_BASE;

	template<bool log = false>
	uint32_t read(addr_t addr, void *opaque);
	template<bool log = false>
	void write(addr_t addr, const uint32_t data, void *opaque);
}
