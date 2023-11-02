// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "nxbx.hpp"
#include "io.hpp"
#include "kernel.hpp"
#include "pit.hpp"
#include <cinttypes>
#include <assert.h>


static uint32_t io_id;

uint32_t
nboxkrnl_read_handler(addr_t addr, void *opaque)
{
	static uint64_t curr_time;

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

	case IO_CHECK_ENQUEUE:
		return pending_packets;

	case IO_QUERY_STATUS:
		// The following two are read in succession with interrupts disabled
		return query_io_packet(io_id, true);

	case IO_QUERY_INFO:
		return query_io_packet(io_id, false);

	case XE_DVD_XBE_LENGTH:
		return (uint32_t)xbe_name.size();

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

	case IO_START:
		enqueue_io_packet(value);
		break;

	case IO_RETRY:
		flush_pending_packets();
		break;

	case IO_SET_ID:
		// This is set together with IO_QUERY_STATUS and IO_QUERY_INFO with interrupts disabled
		io_id = value;
		break;

	case XE_DVD_XBE_ADDR:
		mem_write_block_virt(static_cast<cpu_t *>(opaque), value, (uint32_t)xbe_name.size(), xbe_name.c_str());
		break;

	default:
		logger(log_lv::warn, "%s: unexpected I/O write at port 0x%" PRIX16, __func__, addr);
	}
}
