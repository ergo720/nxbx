// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "logger.hpp"
#include "kernel.hpp"
#include "pit.hpp"
#include <cinttypes>


static uint64_t curr_time;

uint32_t
nboxkrnl_read_handler(addr_t addr, void *opaque)
{
	switch (addr)
	{
	case SYS_TYPE:
		// For now, we always want an xbox system. 0: xbox, 1: chihiro, 2: devkit
		return 0;

	case BOOT_TIME_LOW:
		// These three are read in succession from the clock isr with interrupts disabled, so we can read the boot time only once instead of three times
		curr_time = get_now();
		return static_cast<uint32_t>(curr_time / 100);

	case BOOT_TIME_HIGH:
		return (curr_time / 100) >> 32;

	case BOOT_TIME_MS:
		return static_cast<uint32_t>(curr_time / 1000);

	default:
		logger(log_lv::warn, "%s: unexpected I/O read at port 0x%" PRIX16, __func__, addr);
	}

	return std::numeric_limits<uint32_t>::max();
}

void
nboxkrnl_write_handler(addr_t addr, const uint32_t value, void *opaque)
{
	switch (addr)
	{
	case DBG_STR: {
		// The debug strings from nboxkrnl are 512 byte long at most
		// Also, they might not be contiguous in physical memory, so we use mem_read_block_virt to avoid issues with allocations spanning pages
		uint8_t buff[512];
		mem_read_block_virt(static_cast<cpu_t *>(opaque), value, sizeof(buff), buff);
		logger(log_lv::info, "Received a new debug string from kernel:\n%s", buff);
	}
	break;

	case ABORT:
		cpu_exit(static_cast<cpu_t *>(opaque));
		break;

	default:
		logger(log_lv::warn, "%s: unexpected I/O write at port 0x%" PRIX16, __func__, addr);
	}
}
