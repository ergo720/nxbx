// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "nxbx.hpp"
#include "kernel.hpp"
#include "pit.hpp"
#include <cinttypes>
#include <fstream>
#include <assert.h>

#define XBE_HANDLE -3


// These definitions are the same used by nboxkrnl to submit I/O request, and should be kept synchronized with those
enum io_status {
	no_io_pending = -3,
	not_found,
	error,
	success
};

enum io_request_type {
	open = 0,
	close,
	read,
	write
};

struct io_request {
	int32_t type;
	uint32_t handle;
	uint32_t offset;
	uint32_t size;
	uint32_t address;
};

struct io_info_block {
	io_status status;
	uint32_t info;
};

static uint64_t curr_time;
static io_request curr_io_request = { open, 0, 0, 0, 0 };
static io_info_block io_result = { no_io_pending , 0 };
static std::unordered_map<uint32_t, std::fstream> xbox_handle_map;
static std::vector<char> io_buffer;

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

	case IO_QUERY_STATUS:
		// The following two are read in succession with interrupts disabled
		return io_result.status;

	case IO_QUERY_INFO:
		io_result.status = no_io_pending;
		return io_result.info;

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

	case IO_REQUEST_TYPE:
		// The following five are written in succession with interrupts disabled, so we can cache all five in a single request
		curr_io_request.type = static_cast<int32_t>(value);
		break;

	case XE_LOAD_XBE:
		curr_io_request.handle = value;
		break;

	case IO_OFFSET:
		curr_io_request.offset = value;
		break;

	case IO_SIZE:
		curr_io_request.size = value;
		break;

	case IO_ADDR: {
		// NOTE: perhaps do the actual host I/O on a separate thread instead of here?
		curr_io_request.address = value;
		assert(curr_io_request.handle == XBE_HANDLE);
		auto it = xbox_handle_map.find(curr_io_request.handle);
		if (it == xbox_handle_map.end()) {
			std::fstream fs(xbe_path.c_str(), std::ios_base::in | std::ios_base::out | std::ios_base::binary);
			if (!fs.is_open()) {
				io_result.status = not_found;
				io_result.info = 0;
				break;
			}
			it = xbox_handle_map.emplace(curr_io_request.handle, std::move(fs)).first;
		}

		std::fstream *fs = &it->second;
		switch (curr_io_request.type)
		{
		case io_request_type::open:
			io_result.status = success;
			io_result.info = 0;
			break;

		case io_request_type::close:
			xbox_handle_map.erase(curr_io_request.handle);
			io_result.status = success;
			io_result.info = 0;
			break;

		case io_request_type::read:
			fs->seekg(curr_io_request.offset, fs->beg);
			if (io_buffer.size() < curr_io_request.size) {
				io_buffer.resize(curr_io_request.size);
			}
			fs->read(io_buffer.data(), curr_io_request.size);
			io_result.status = fs->rdstate() == std::ios_base::goodbit ? success : error;
			io_result.info = static_cast<uint32_t>(fs->gcount());
			mem_write_block_virt(static_cast<cpu_t *>(opaque), curr_io_request.address, curr_io_request.size, io_buffer.data());
			break;

		case io_request_type::write:
			fs->seekg(curr_io_request.offset, fs->beg);
			if (io_buffer.size() < curr_io_request.size) {
				io_buffer.resize(curr_io_request.size);
			}
			mem_read_block_virt(static_cast<cpu_t *>(opaque), curr_io_request.address, curr_io_request.size, reinterpret_cast<uint8_t *>(io_buffer.data()));
			fs->write(io_buffer.data(), curr_io_request.size);
			io_result.status = fs->rdstate() == std::ios_base::goodbit ? success : error;
			io_result.info = 0;
			break;

		default:
			logger(log_lv::warn, "%s: unknown I/O request of type %" PRId32, __func__, curr_io_request.type);
		}

	}
	break;

	default:
		logger(log_lv::warn, "%s: unexpected I/O write at port 0x%" PRIX16, __func__, addr);
	}
}
