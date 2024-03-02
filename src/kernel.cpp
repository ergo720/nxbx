// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "console.hpp"
#include "nxbx.hpp"
#include "io.hpp"
#include "kernel.hpp"
#include "pit.hpp"
#include "clock.hpp"
#include <cinttypes>
#include <assert.h>


namespace kernel {
	static uint64_t lost_clock_increment, last_us, curr_us;

	static uint64_t
	calculate_clock_increment()
	{
		// NOTE: a clock interrupt is generated at every ms, so ideally the increment should always be 10000 -> 10000 * 100ns units = 1ms
		curr_us = timer::get_now();
		uint64_t elapsed_us = curr_us - last_us;
		last_us = curr_us;
		uint64_t elapsed_clock_increment = elapsed_us * 10;
		lost_clock_increment += elapsed_clock_increment;
		uint64_t actual_clock_increment = ((lost_clock_increment / 10000) * 10000); // floor to the nearest multiple of clock increment
		lost_clock_increment -= actual_clock_increment;

		return actual_clock_increment;
	}

	uint32_t
	read_handler(addr_t addr, void *opaque)
	{
		static uint64_t acpi_time, curr_clock_increment;

		switch (addr)
		{
		case SYS_TYPE:
			// For now, we always want an xbox system. 0: xbox, 1: chihiro, 2: devkit
			return 0;

		case CLOCK_INCREMENT_LOW:
			// These three are read in succession from the clock isr with interrupts disabled, so we can read the boot time only once instead of three times
			curr_clock_increment = calculate_clock_increment();
			return static_cast<uint32_t>(curr_clock_increment);

		case CLOCK_INCREMENT_HIGH:
			return curr_clock_increment >> 32;

		case BOOT_TIME_MS:
			return static_cast<uint32_t>(curr_us / 1000);

		case IO_CHECK_ENQUEUE:
			return io::pending_packets;

		case XE_DVD_XBE_LENGTH:
			return (uint32_t)io::xbe_path.size();

		case ACPI_TIME_LOW:
			// These two are read in succession from KeQueryPerformanceCounter with interrupts disabled, so we can read the ACPI time only once instead of two times
			acpi_time = timer::get_acpi_now();
			return static_cast<uint32_t>(acpi_time);

		case ACPI_TIME_HIGH:
			return acpi_time >> 32;

		default:
			logger(log_lv::warn, "%s: unexpected I/O read at port 0x%" PRIX16, __func__, addr);
		}

		return 0;
	}

	void
	write_handler(addr_t addr, const uint32_t value, void *opaque)
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
			console::get().exit();
			break;

		case IO_START:
			io::submit_io_packet(value);
			break;

		case IO_RETRY:
			io::flush_pending_packets();
			break;

		case IO_QUERY:
			io::query_io_packet(value);
			break;

		case XE_DVD_XBE_ADDR:
			mem_write_block_virt(static_cast<cpu_t *>(opaque), value, (uint32_t)io::xbe_path.size(), io::xbe_path.c_str());
			break;

		default:
			logger(log_lv::warn, "%s: unexpected I/O write at port 0x%" PRIX16, __func__, addr);
		}
	}
}
