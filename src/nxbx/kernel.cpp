// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#include "lib86cpu.hpp"
#include "console.hpp"
#include "io.hpp"
#include "kernel.hpp"
#include "pit.hpp"
#include "clock.hpp"
#include "paths.hpp"
#include <cinttypes>
#include <assert.h>

#define MODULE_NAME kernel


namespace kernel
{
	static uint64_t s_lost_clock_increment, s_last_us, s_curr_us;

	static uint64_t
	calculate_clock_increment()
	{
		// NOTE: a clock interrupt is generated at every ms, so ideally the increment should always be 10000 -> 10000 * 100ns units = 1ms
		s_curr_us = timer::get_now();
		uint64_t elapsed_us = s_curr_us - s_last_us;
		s_last_us = s_curr_us;
		uint64_t elapsed_clock_increment = elapsed_us * 10;
		s_lost_clock_increment += elapsed_clock_increment;
		uint64_t actual_clock_increment = ((s_lost_clock_increment / 10000) * 10000); // floor to the nearest multiple of clock increment
		s_lost_clock_increment -= actual_clock_increment;

		return actual_clock_increment;
	}

	uint32_t read32(uint32_t addr, void *opaque)
	{
		static uint64_t s_acpi_time, s_curr_clock_increment;
		uint32_t value = 0;

		switch (addr)
		{
		case MACHINE_TYPE:
			value = std::to_underlying<console_t>(g_console->get_boot_params().console_type);
			break;

		case CLOCK_INCREMENT_LOW:
			// These three are read in succession from the clock isr with interrupts disabled, so we can read the boot time only once instead of three times
			s_curr_clock_increment = calculate_clock_increment();
			value = static_cast<uint32_t>(s_curr_clock_increment);
			break;

		case CLOCK_INCREMENT_HIGH:
			value = s_curr_clock_increment >> 32;
			break;

		case BOOT_TIME_MS:
			value = static_cast<uint32_t>(s_curr_us / 1000);
			break;

		case IO_CHECK_ENQUEUE:
			value = io::g_pending_packets;
			break;

		case XE_DVD_XBE_LENGTH:
			value = (uint32_t)emu_path::g_xbe_path_xbox.size();
			break;

		case ACPI_TIME_LOW:
			// These two are read in succession from KeQueryPerformanceCounter with interrupts disabled, so we can read the ACPI time only once instead of two times
			s_acpi_time = timer::get_acpi_now();
			value = static_cast<uint32_t>(s_acpi_time);
			break;

		case ACPI_TIME_HIGH:
			value = s_acpi_time >> 32;
			break;
		}

		return value;
	}

	void write32(uint32_t addr, const uint32_t value, void *opaque)
	{
		switch (addr)
		{
		case DBG_STR: {
			// The debug strings from nboxkrnl are 512 byte long at most
			// Also, they might not be contiguous in physical memory, so we use mem_read_block_virt to avoid issues with allocations spanning pages
			uint8_t buff[512];
			mem_read_block_virt(static_cast<cpu_t *>(opaque), value, sizeof(buff), buff);
			logger_en(info, "%s", buff);
		}
		break;

		case ABORT:
			g_console->exit();
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
			mem_write_block_virt(static_cast<cpu_t *>(opaque), value, (uint32_t)emu_path::g_xbe_path_xbox.size(), emu_path::g_xbe_path_xbox.c_str());
			break;
		}
	}
}
