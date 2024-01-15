// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "nxbx.hpp"
#include "io.hpp"
#include "kernel.hpp"
#include "pit.hpp"
#include "clock.hpp"
#include <cinttypes>
#include <assert.h>


static uint64_t io_id, lost_clock_increment, last_us, curr_us;

static uint64_t
calculate_kernel_clock_increment()
{
	// NOTE: a clock interrupt is generated at every ms, so ideally the increment should always be 10000 -> 10000 * 100ns units = 1ms
	curr_us = get_now();
	uint64_t elapsed_us = curr_us - last_us;
	last_us = curr_us;
	uint64_t elapsed_clock_increment = elapsed_us * 10;
	lost_clock_increment += elapsed_clock_increment;
	uint64_t actual_clock_increment = ((lost_clock_increment / 10000) * 10000); // floor to the nearest multiple of clock increment
	lost_clock_increment -= actual_clock_increment;

	return actual_clock_increment;
}

uint32_t
nboxkrnl_read_handler(addr_t addr, void *opaque)
{
	static uint64_t acpi_time, curr_clock_increment;

	switch (addr)
	{
	case SYS_TYPE:
		// For now, we always want an xbox system. 0: xbox, 1: chihiro, 2: devkit
		return 0;

	case CLOCK_INCREMENT_LOW:
		// These three are read in succession from the clock isr with interrupts disabled, so we can read the boot time only once instead of three times
		curr_clock_increment = calculate_kernel_clock_increment();
		return static_cast<uint32_t>(curr_clock_increment);

	case CLOCK_INCREMENT_HIGH:
		return curr_clock_increment >> 32;

	case BOOT_TIME_MS:
		return static_cast<uint32_t>(curr_us / 1000);

	case IO_CHECK_ENQUEUE:
		return pending_packets;

	case IO_QUERY_STATUS:
		// The following two are read in succession with interrupts disabled
		return query_io_packet(io_id, true);

	case IO_QUERY_INFO:
		return query_io_packet(io_id, false);

	case XE_DVD_XBE_LENGTH:
		return (uint32_t)xbox_xbe_path.size();

	case ACPI_TIME_LOW:
		// These two are read in succession from KeQueryPerformanceCounter with interrupts disabled, so we can read the ACPI time only once instead of two times
		acpi_time = get_acpi_now();
		return static_cast<uint32_t>(acpi_time);

	case ACPI_TIME_HIGH:
		return acpi_time >> 32;

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
		logger(log_lv::info, "Kernel says: %s", buff);
	}
	break;

	case ABORT:
		cpu_exit(static_cast<cpu_t *>(opaque));
		break;

	case IO_START:
		submit_io_packet(value);
		break;

	case IO_RETRY:
		flush_pending_packets();
		break;

	case IO_SET_ID_LOW:
		// This and the following are set together with IO_QUERY_STATUS and IO_QUERY_INFO with interrupts disabled
		io_id = value;
		break;

	case IO_SET_ID_HIGH:
		io_id |= (static_cast<uint64_t>(value) << 32);
		break;

	case XE_DVD_XBE_ADDR:
		mem_write_block_virt(static_cast<cpu_t *>(opaque), value, (uint32_t)xbox_xbe_path.size(), xbox_xbe_path.c_str());
		break;

	default:
		logger(log_lv::warn, "%s: unexpected I/O write at port 0x%" PRIX16, __func__, addr);
	}
}
