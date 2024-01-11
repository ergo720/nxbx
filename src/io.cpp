// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "io.hpp"
#include "logger.hpp"
#include "cpu.hpp"
#include "eeprom.hpp"
#include "kernel.hpp"
#include "util.hpp"
#include "xpartition.hpp"
#include <thread>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <cinttypes>
#include <cassert>
#include <array>

// Device number
#define DEV_CDROM      0
#define DEV_EEPROM     1
#define DEV_PARTITION0 2
#define DEV_PARTITION1 3
#define DEV_PARTITION2 4
#define DEV_PARTITION3 5
#define DEV_PARTITION4 6
#define DEV_PARTITION5 7
#define DEV_PARTITION6 8 // non-standard
#define DEV_PARTITION7 9 // non-standard
#define NUM_OF_DEVS    10

// Special internal handles used by the kernel
#define XBE_HANDLE        DEV_CDROM
#define EEPROM_HANDLE     DEV_EEPROM
#define PARTITION0_HANDLE DEV_PARTITION0
#define PARTITION1_HANDLE DEV_PARTITION1
#define PARTITION2_HANDLE DEV_PARTITION2
#define PARTITION3_HANDLE DEV_PARTITION3
#define PARTITION4_HANDLE DEV_PARTITION4
#define PARTITION5_HANDLE DEV_PARTITION5
#define PARTITION6_HANDLE DEV_PARTITION6 // non-standard
#define PARTITION7_HANDLE DEV_PARTITION7 // non-standard
#define FIRST_FREE_HANDLE NUM_OF_DEVS

// Disposition flags (same as used by NtCreate/OpenFile)
#define IO_SUPERSEDE    0
#define IO_OPEN         1
#define IO_CREATE       2
#define IO_OPEN_IF      3
#define IO_OVERWRITE    4
#define IO_OVERWRITE_IF 5

#define IO_GET_TYPE(type) (io_request_type)((uint32_t)(type) & 0xF0000000)
#define IO_GET_FLAGS(type) ((uint32_t)(type) & 0x007FFFF8)
#define IO_GET_DISPOSITION(type) ((uint32_t)(type) & 0x00000007)
#define IO_GET_DEV(type) (((uint32_t)(type) >> 23) & 0x0000001F)

// These definitions are the same used by nboxkrnl to submit I/O request, and should be kept synchronized with those
enum io_status : int32_t {
	success = 0,
	pending,
	error,
	failed,
	is_a_directory,
	not_a_directory,
	not_found
};

enum io_request_type : uint32_t {
	open    = 1 << 28,
	remove1 = 2 << 28,
	close   = 3 << 28,
	read    = 4 << 28,
	write   = 5 << 28
};

enum io_flags : uint32_t {
	is_directory      = 1 << 3,
	must_be_a_dir     = 1 << 4,
	must_not_be_a_dir = 1 << 5
};

enum io_info : uint32_t {
	no_data = 0,
	superseded = 0,
	opened,
	created,
	overwritten,
	exists,
	not_exists
};

// Type layout of io_request
// io_request_type - dev_type - io_flags - disposition
// 31 - 28           27 - 23    22 - 3     2 - 0

// Host version of io_request
struct io_request {
	~io_request();
	uint64_t id; // unique id to identify this request
	io_request_type type; // type of request and flags
	int64_t offset; // file offset from which to start the I/O
	uint32_t size; // bytes to transfer or size of path for open/create requests
	union {
		// virtual address of the data to transfer or file handle for open/create requests
		uint64_t handle_oc;
		uint32_t address;
	};
	union {
		// file handle or file path for open/create requests
		uint64_t handle;
		char *path;
	};
};

// io_request as used in nboxkrnl, also packed to make sure it has the same padding and alignment
#pragma pack(1)
struct packed_io_request {
	uint64_t id;
	io_request_type type;
	int64_t offset;
	uint32_t size;
	uint64_t handle_or_address;
	uint64_t handle_or_path;
};
#pragma pack()

struct io_info_block {
	io_status status;
	io_info info;
};

io_request::~io_request()
{
	if (IO_GET_TYPE(this->type) == open) {
		delete[] this->path;
		this->path = nullptr;
	}
}

static std::deque< std::unique_ptr<io_request>> curr_io_queue;
static std::vector<std::unique_ptr<io_request>> pending_io_vec;
static std::unordered_map<uint64_t, std::unique_ptr<io_info_block>> completed_io_info;
static std::array<std::unordered_map<uint64_t, std::pair<std::fstream, xbox_string>>, NUM_OF_DEVS> xbox_handle_map;
static std::mutex queue_mtx;
static std::mutex completed_io_mtx;
static std::vector<char> io_buffer;
static std::atomic_flag io_pending;
static std::atomic_flag io_running;

static std::filesystem::path hdd_path;
static std::filesystem::path dvd_path;
static std::filesystem::path eeprom_path;


static bool
io_open_special_files()
{
	const auto &lambda = [](std::filesystem::path resolved_path, uint64_t handle) -> bool {
		if (auto opt = open_file(resolved_path); !opt) {
			return false;
		}
		else {
			auto pair = xbox_handle_map[handle].emplace(handle, std::make_pair(std::move(*opt),
				traits_cast<xbox_char_traits, char, std::char_traits<char>>(resolved_path.string())));
			assert(pair.second == true);
			return true;
		}
		};

	if (!lambda((eeprom_path / "eeprom.bin").make_preferred(), EEPROM_HANDLE)) {
		return false;
	}

	for (unsigned i = 0; i < XBOX_NUM_OF_PARTITIONS; ++i) {
		std::filesystem::path curr_partition_dir = hdd_path / ("Partition" + std::to_string(i) + ".bin");
		curr_partition_dir.make_preferred();
		if (!lambda(curr_partition_dir, PARTITION0_HANDLE + i)) {
			return false;
		}
	}

	return true;
}

static std::filesystem::path
io_parse_path(io_request *curr_io_request)
{
	// NOTE1: Paths from the kernel should have the form "\device\<device name>\<partition number (optional)>\<file name>"
	// "device name" can be CdRom0, Harddisk0 and "partition number" can be Partition0, Partition1, ...
	// NOTE2: path comparisons are case-insensitive in the xbox kernel, so we need to do the same
	// TODO: this should also resolve symbolic links

	xbox_string_view path(curr_io_request->path, curr_io_request->size);
	assert(path.starts_with('\\'));
	size_t dev_pos = path.find_first_of('\\', 1); // discards "device"
	assert(dev_pos != xbox_string_view::npos);
	size_t pos = path.find_first_of('\\', dev_pos + 1);
	assert(pos != xbox_string_view::npos);
	xbox_string_view device = path.substr(dev_pos + 1, pos - dev_pos - 1);
	std::filesystem::path resolved_path;
	if (device.compare("CdRom0") == 0) {
		resolved_path = dvd_path;
	}
	else {
		resolved_path = hdd_path;
		size_t pos2 = path.find_first_of('\\', pos + 1);
		assert(pos != xbox_string_view::npos);
		xbox_string_view partition = path.substr(pos + 1, pos2);
		resolved_path /= partition;
		pos = pos2;
	}
	xbox_string_view name = path.substr(pos + 1);
	resolved_path /= name;
	resolved_path.make_preferred();

	return resolved_path;
}

static void
io_thread()
{
	io_running.test_and_set();

	while (true) {

		// Wait until there's some work to do
		io_pending.wait(false);

		// Check to see if we need to terminate this thread
		if (io_running.test() == false) [[unlikely]] {
			pending_packets = false;
			curr_io_queue.clear();
			completed_io_info.clear();
			for (auto &handle_map : xbox_handle_map) {
				handle_map.clear();
			}
			pending_io_vec.clear();
			io_running.test_and_set();
			io_running.notify_one();
			return;
		}

		queue_mtx.lock();
		if (curr_io_queue.empty()) {
			io_pending.clear();
			queue_mtx.unlock();
			continue;
		}
		std::unique_ptr<io_request> curr_io_request = std::move(curr_io_queue.front());
		curr_io_queue.pop_front();
		queue_mtx.unlock();

		io_request_type io_type = IO_GET_TYPE(curr_io_request->type);
		uint32_t dev = IO_GET_DEV(curr_io_request->type);
		if (io_type == open) {
			// This code opens/creates the file according to the CreateDisposition parameter used by NtCreate/OpenFile

			std::filesystem::path resolved_path = io_parse_path(curr_io_request.get());
			std::unique_ptr<io_info_block> io_result = std::make_unique<io_info_block>(error, no_data);
			uint32_t disposition = IO_GET_DISPOSITION(curr_io_request->type);
			uint32_t flags = IO_GET_FLAGS(curr_io_request->type);
			bool host_is_directory, is_directory = flags & io_flags::is_directory;

			const auto add_to_map = [&curr_io_request, &resolved_path, dev](auto &&opt, std::unique_ptr<io_info_block> *io_result) {
				auto pair = xbox_handle_map[dev].emplace(curr_io_request->handle_oc, std::make_pair(std::move(*opt),
					traits_cast<xbox_char_traits, char, std::char_traits<char>>(resolved_path.string())));
				assert(pair.second == true);
				io_result->get()->status = success;
				};
			const auto check_dir_flags = [flags](bool host_is_directory, std::unique_ptr<io_info_block> *io_result) -> bool {
				if ((flags & must_be_a_dir) && !host_is_directory) {
					io_result->get()->status = not_a_directory;
					return false;
				}
				else if ((flags & must_not_be_a_dir) && host_is_directory) {
					io_result->get()->status = is_a_directory;
					return false;
				}
				else {
					return true;
				}
				};

			if (file_exists(resolved_path, &host_is_directory)) {
				io_result->info = exists;
				if (disposition == IO_CREATE) {
					// Create if doesn't exist - FILE_CREATE
					io_result->status = failed;
					io_result->info = exists;
				}
				else if ((disposition == IO_OPEN) || (disposition == IO_OPEN_IF)) {
					// Open if exists - FILE_OPEN
					// Open always - FILE_OPEN_IF
					if (check_dir_flags(host_is_directory, &io_result)) {
						if (is_directory) {
							// Open directory: nothing to do
							io_result->info = opened;
							add_to_map(std::make_optional<std::fstream>(), &io_result);
						}
						else {
							// Open file
							if (auto opt = open_file(resolved_path); opt) {
								io_result->info = opened;
								add_to_map(opt, &io_result);
							}
						}
					}
				}
				else {
					// Create always - FILE_SUPERSEDE
					// Truncate if exists - FILE_OVERWRITE
					// Truncate always - FILE_OVERWRITE_IF
					if (check_dir_flags(host_is_directory, &io_result)) {
						if (is_directory) {
							// Create directory: already exists
							io_result->info = exists;
							add_to_map(std::make_optional<std::fstream>(), &io_result);
						}
						else {
							// Create file
							if (auto opt = create_file(resolved_path); opt) {
								io_result->info = (disposition == IO_SUPERSEDE) ? superseded : overwritten;
								add_to_map(opt, &io_result);
							}
						}
					}
				}
			}
			else {
				io_result->info = not_exists;
				if ((disposition == IO_CREATE) || (disposition == IO_SUPERSEDE) || (disposition == IO_OPEN_IF) || (disposition == IO_OVERWRITE_IF)) {
					// Create if doesn't exist - FILE_CREATE
					// Create always - FILE_SUPERSEDE
					// Open always - FILE_OPEN_IF
					// Truncate always - FILE_OVERWRITE_IF
					if (is_directory) {
						// Create directory
						if (::create_directory(resolved_path)) {
							io_result->info = created;
							add_to_map(std::make_optional<std::fstream>(), &io_result);
						}
					}
					else {
						// Create file
						if (auto opt = create_file(resolved_path); opt) {
							io_result->info = created;
							add_to_map(opt, &io_result);
						}
					}
				}
				else {
					// Open if exists - FILE_OPEN
					// Truncate if exists - FILE_OVERWRITE
					io_result->status = not_found;
					io_result->info = not_exists;
				}
			}

			completed_io_mtx.lock();
			completed_io_info.emplace(curr_io_request->id, std::move(io_result));
			completed_io_mtx.unlock();
			continue;
		}

		auto it = xbox_handle_map[dev].find(curr_io_request->handle);
		if (it == xbox_handle_map[dev].end()) [[unlikely]] {
			logger(log_lv::warn, "Xbox handle %" PRIu32 " not found", it->first); // this should not happen...
			completed_io_mtx.lock();
			completed_io_info.emplace(curr_io_request->id, std::make_unique<io_info_block>(error, no_data));
			completed_io_mtx.unlock();
			continue;
		}

		std::fstream *fs = &it->second.first;
		std::unique_ptr<io_info_block> io_result = std::make_unique<io_info_block>(success, no_data);
		switch (io_type)
		{
		case io_request_type::close:
			xbox_handle_map[dev].erase(curr_io_request->handle);
			break;

		case io_request_type::read:
			if (!fs->is_open()) [[unlikely]] {
				// Read operation on a directory (this should not happen...)
				logger(log_lv::warn, "Read operation to directory handle %" PRIu32 " with path %s", it->first, it->second.second.c_str());
				io_result->status = error;
				io_result->info = no_data;
				break;
			}
			fs->seekg(curr_io_request->offset, fs->beg);
			if (io_buffer.size() < curr_io_request->size) {
				io_buffer.resize(curr_io_request->size);
			}
			fs->read(io_buffer.data(), curr_io_request->size);
			if (fs->good()) {
				io_result->info = static_cast<io_info>(fs->gcount());
				mem_write_block_virt(g_cpu, curr_io_request->address, curr_io_request->size, io_buffer.data());
			}
			else {
				io_result->status = error;
				fs->clear();
			}
			break;

		case io_request_type::write:
			if (!fs->is_open()) [[unlikely]] {
				// Write operation on a directory (this should not happen...)
				logger(log_lv::warn, "Write operation to directory handle %" PRIu32 " with path %s", it->first, it->second.second.c_str());
				io_result->status = error;
				io_result->info = no_data;
				break;
			}
			fs->seekg(curr_io_request->offset, fs->beg);
			if (io_buffer.size() < curr_io_request->size) {
				io_buffer.resize(curr_io_request->size);
			}
			mem_read_block_virt(g_cpu, curr_io_request->address, curr_io_request->size, reinterpret_cast<uint8_t *>(io_buffer.data()));
			fs->write(io_buffer.data(), curr_io_request->size);
			if (fs->good()) {
				io_result->status = error;
				fs->clear();
			}
			break;

		case io_request_type::remove1: // TODO
		default:
			logger(log_lv::warn, "Unknown I/O request of type %" PRId32, curr_io_request->type);
		}

		completed_io_mtx.lock();
		completed_io_info.emplace(curr_io_request->id, std::move(io_result));
		completed_io_mtx.unlock();
	}
}

static void
enqueue_io_packet(std::unique_ptr<io_request> curr_io_request)
{
	// If the I/O thread is currently holding the lock, we won't wait and instead retry the operation later
	if (queue_mtx.try_lock()) {
		curr_io_queue.push_back(std::move(curr_io_request));
		// Signal that there's a new packet to process
		io_pending.test_and_set();
		io_pending.notify_one();
		queue_mtx.unlock();
	}
	else {
		pending_io_vec.push_back(std::move(curr_io_request));
		pending_packets = true;
	}
}

void
submit_io_packet(uint32_t addr)
{
	packed_io_request packed_curr_io_request;
	std::unique_ptr<io_request> curr_io_request = std::make_unique<io_request>();
	mem_read_block_virt(g_cpu, addr, sizeof(packed_io_request), (uint8_t *)&packed_curr_io_request);
	curr_io_request->id = packed_curr_io_request.id;
	curr_io_request->type = packed_curr_io_request.type;
	curr_io_request->handle_oc = packed_curr_io_request.handle_or_address;
	curr_io_request->offset = packed_curr_io_request.offset;
	curr_io_request->size = packed_curr_io_request.size;
	curr_io_request->handle = packed_curr_io_request.handle_or_path;

	if (IO_GET_TYPE(curr_io_request->type) == open) {
		char *path = new char[curr_io_request->size];
		mem_read_block_virt(g_cpu, (addr_t)curr_io_request->handle, curr_io_request->size, (uint8_t *)path);
		curr_io_request->path = path;
	}

	enqueue_io_packet(std::move(curr_io_request));
}

void
flush_pending_packets()
{
	if (!pending_io_vec.empty()) {
		// If the I/O thread is currently holding the lock, we won't wait and instead retry the operation later
		if (queue_mtx.try_lock()) {
			std::for_each(pending_io_vec.begin(), pending_io_vec.end(), [](auto &&packet) {
				curr_io_queue.push_back(std::move(packet));
				});
			pending_io_vec.clear();
			pending_packets = false;
			// Signal that there are new packets to process
			io_pending.test_and_set();
			io_pending.notify_one();
			queue_mtx.unlock();
		}
	}
}

uint32_t
query_io_packet(uint64_t id, bool query_status)
{
	static io_info_block block;

	if (query_status) {
		block.status = pending;
		if (completed_io_mtx.try_lock()) { // don't wait if the I/O thread is currently using the map
			auto it = completed_io_info.find(id);
			if (it != completed_io_info.end()) {
				block.status = it->second->status;
				block.info = it->second->info;
				completed_io_info.erase(it);
			}
			completed_io_mtx.unlock();
		}

		return block.status;
	}

	return block.info;
}

bool
io_init(const char *nxbx_path, const char *xbe_path)
{
	std::filesystem::path curr_dir = nxbx_path;
	curr_dir = curr_dir.remove_filename();
	std::filesystem::path hdd_dir = curr_dir;
	hdd_dir /= "Harddisk/";
	hdd_dir.make_preferred();
	if (!::create_directory(hdd_dir)) {
		return false;
	}
	for (unsigned i = 0; i < XBOX_NUM_OF_PARTITIONS; ++i) {
		std::filesystem::path curr_partition_dir = hdd_dir / ("Partition" + std::to_string(i));
		curr_partition_dir.make_preferred();
		if (i) {
			if (!::create_directory(curr_partition_dir)) {
				return false;
			}
		}
		if (!create_partition_metadata_file(curr_partition_dir, i)) {
			return false;
		}
	}
	std::filesystem::path eeprom_dir = curr_dir;
	eeprom_dir /= "eeprom.bin";
	eeprom_dir.make_preferred();
	if (!file_exists(eeprom_dir)) {
		if (auto opt = create_file(eeprom_dir); !opt) {
			return false;
		}
		else {
			if (!gen_eeprom(std::move(opt.value()))) {
				return false;
			}
		}
	}

	std::filesystem::path local_xbe_path = std::filesystem::path(xbe_path).make_preferred();
	xbe_name = traits_cast<xbox_char_traits, char, std::char_traits<char>>(local_xbe_path.filename().string());
	hdd_path = hdd_dir;
	dvd_path = local_xbe_path.remove_filename();
	eeprom_path = eeprom_dir.remove_filename();

	if (!io_open_special_files()) {
		return false;
	}

	std::thread(io_thread).detach();

	return true;
}

void
io_stop()
{
	if (io_running.test()) {
		// Signal the I/O thread that it needs to exit
		io_running.clear();
		queue_mtx.lock();
		io_pending.test_and_set();
		io_pending.notify_one();
		queue_mtx.unlock();
		io_running.wait(false);
		io_running.clear();
	}
}
