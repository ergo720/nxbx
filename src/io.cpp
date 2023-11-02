// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "io.hpp"
#include "logger.hpp"
#include "files.hpp"
#include "cpu.hpp"
#include "kernel.hpp"
#include <thread>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <cinttypes>
#include <cassert>

#define XBE_HANDLE -3

#define IO_DIRECTORY 1U
#define IO_ALWAYS 2U
#define IO_TRUNCATE 4U

#define IO_GET_TYPE(type) (io_request_type)(((uint32_t)(type)) & 0xFFFF0000)
#define IO_GET_DEVICE(type) (io_dev)(((uint32_t)(type)) & 0x0000F000)
#define IO_IS_DIRECTORY(type) ((uint32_t)(type) & IO_DIRECTORY)
#define IO_IS_ALWAYS(type) ((uint32_t)(type) & IO_ALWAYS)
#define IO_IS_TRUNCATE(type) ((uint32_t)(type) & IO_TRUNCATE)

// These definitions are the same used by nboxkrnl to submit I/O request, and should be kept synchronized with those
enum io_status : uint32_t {
	success = 0,
	pending,
	error
};

enum io_request_type : uint32_t {
	open = 1 << 16,
	create = 2 << 16,
	remove1 = 3 << 16,
	close = 4 << 16,
	read = 5 << 16,
	write = 6 << 16
};

enum io_dev : uint32_t {
	dvd = 0 << 12,
	hdd = 1 << 12
};

// Host version of io_request
struct io_request {
	~io_request();
	uint32_t id; // unique id to identify this request
	io_request_type type; // type of request and flags
	int64_t offset; // file offset from which to start the I/O
	uint32_t size; // bytes to transfer or size of path for open/create requests
	union {
		// virtual address of the data to transfer or file handle for open/create requests
		uint32_t handle_oc;
		uint32_t address;
	};
	union {
		// file handle or file path for open/create requests
		uint32_t handle;
		char *path;
	};
};

// io_request as used in nboxkrnl, also packed to make sure it has the same padding and alignment
#pragma pack(1)
struct packed_io_request {
	uint32_t id;
	io_request_type type;
	int64_t offset;
	uint32_t size;
	uint32_t handle_or_address;
	uint32_t handle_or_path;
};
#pragma pack()

struct io_info_block {
	io_status status;
	uint32_t info;
};

io_request::~io_request()
{
	if ((IO_GET_TYPE(this->type) == open) || (IO_GET_TYPE(this->type) == create)) {
		delete[] this->path;
		this->path = nullptr;
	}
}

static std::deque< std::unique_ptr<io_request>> curr_io_queue;
static std::vector<std::unique_ptr<io_request>> pending_io_vec;
static std::unordered_map<uint32_t, std::unique_ptr<io_info_block>> completed_io_info;
static std::unordered_map<uint32_t, std::pair<std::fstream, std::string>> xbox_handle_map;
static std::mutex queue_mtx;
static std::mutex completed_io_mtx;
static std::vector<char> io_buffer;
static std::atomic_flag io_pending;
static std::atomic_flag io_running;

static std::filesystem::path hdd_path;
static std::filesystem::path dvd_path;


static std::filesystem::path
io_parse_path(io_request *curr_io_request)
{
	// Paths from the kernel should have the form "\<device>\<partition number (optional)>\<file name>"
	// "device" can be CdRom0, Harddisk0 and "partition number" can be Partition0, Partition1, ...

	std::string_view path(curr_io_request->path);
	assert(path.starts_with('\\'));
	size_t pos = path.find_first_of('\\', 1);
	assert(pos != std::string_view::npos);
	std::string_view device = path.substr(1, pos - 1);
	std::filesystem::path resolved_path;
	if (device.compare("CdRom0") == 0) {
		resolved_path = dvd_path;
	}
	else {
		resolved_path = hdd_path;
		size_t pos2 = path.find_first_of('\\', pos + 1);
		assert(pos != std::string_view::npos);
		std::string_view partition = path.substr(pos + 1, pos2);
		resolved_path /= partition;
		resolved_path.make_preferred();
		pos = pos2;
	}
	std::string_view name = path.substr(pos + 1);
	resolved_path /= name;

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
			xbox_handle_map.clear();
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
		if ((io_type == open) || io_type == create) {
			std::filesystem::path resolved_path = io_parse_path(curr_io_request.get());
			if (IO_IS_DIRECTORY(io_type)) {
				if (io_type == open) {
					// Open directory: nothing to do
					completed_io_mtx.lock();
					completed_io_info.emplace(curr_io_request->id, std::make_unique<io_info_block>(success, 0));
					completed_io_mtx.unlock();
					continue;
				}
				else {
					// Create directory
					bool created = ::create_directory(resolved_path);
					completed_io_mtx.lock();
					completed_io_info.emplace(curr_io_request->id, std::make_unique<io_info_block>(created ? success : error, 0));
					completed_io_mtx.unlock();
					continue;
				}
			}
			else {
				// This code opens/creates the file according to the CreateDisposition parameter used by NtCreateFile
				bool opened = false;
				const auto add_to_map = [&curr_io_request, &resolved_path](auto &&opt) -> bool {
					auto pair = xbox_handle_map.emplace(curr_io_request->handle_oc, std::make_pair(std::move(*opt), resolved_path.string()));
					assert(pair.second == true);
					return true;
					};
				if (io_type == open) {
					if (IO_IS_TRUNCATE(curr_io_request->type)) {
						if (IO_IS_ALWAYS(curr_io_request->type)) {
							// Truncate always - FILE_OVERWRITE_IF
							if (auto opt = create_file(resolved_path); opt) {
								opened = add_to_map(opt);
							}
						}
						else {
							// Truncate if exists - FILE_OVERWRITE
							if (file_exists(resolved_path)) {
								if (auto opt = create_file(resolved_path); opt) {
									opened = add_to_map(opt);
								}
							}
						}
					}
					else {
						if (IO_IS_ALWAYS(curr_io_request->type)) {
							// Open always - FILE_OPEN_IF
							if (file_exists(resolved_path)) {
								if (auto opt = open_file(resolved_path); opt) {
									opened = add_to_map(opt);
								}
							}
						}
						else {
							// Open if exists - FILE_OPEN
							if (auto opt = open_file(resolved_path); opt) {
								opened = add_to_map(opt);
							}
						}
					}
				}
				else {
					if (IO_IS_ALWAYS(curr_io_request->type)) {
						// Create always - FILE_SUPERSEDE
						if (auto opt = create_file(resolved_path); opt) {
							opened = add_to_map(opt);
						}
					}
					else {
						// Create if doesn't exist - FILE_CREATE
						if (!file_exists(resolved_path)) {
							if (auto opt = create_file(resolved_path); opt) {
								opened = add_to_map(opt);
							}
						}
					}
				}

				completed_io_mtx.lock();
				completed_io_info.emplace(curr_io_request->id, std::make_unique<io_info_block>(opened ? success : error, 0));
				completed_io_mtx.unlock();
				continue;
			}
		}

		auto it = xbox_handle_map.find((uint32_t)curr_io_request->handle);
		if (it == xbox_handle_map.end()) [[unlikely]] {
			logger(log_lv::warn, "Xbox handle %" PRIu32 " not found"); // this should not happen...
			completed_io_mtx.lock();
			completed_io_info.emplace(curr_io_request->id, std::make_unique<io_info_block>(error, 0));
			completed_io_mtx.unlock();
			continue;
		}

		std::fstream *fs = &it->second.first;
		std::unique_ptr<io_info_block> io_result = std::make_unique<io_info_block>(success, 0);
		switch (curr_io_request->type)
		{
		case io_request_type::close:
			xbox_handle_map.erase(curr_io_request->handle);
			break;

		case io_request_type::read:
			fs->seekg(curr_io_request->offset, fs->beg);
			if (io_buffer.size() < curr_io_request->size) {
				io_buffer.resize(curr_io_request->size);
			}
			fs->read(io_buffer.data(), curr_io_request->size);
			if (fs->rdstate() == std::ios_base::goodbit) {
				io_result->info = static_cast<uint32_t>(fs->gcount());
				mem_write_block_virt(g_cpu, curr_io_request->address, curr_io_request->size, io_buffer.data());
			}
			else {
				io_result->status = error;
				fs->clear();
			}
			break;

		case io_request_type::write:
			fs->seekg(curr_io_request->offset, fs->beg);
			if (io_buffer.size() < curr_io_request->size) {
				io_buffer.resize(curr_io_request->size);
			}
			mem_read_block_virt(g_cpu, curr_io_request->address, curr_io_request->size, reinterpret_cast<uint8_t *>(io_buffer.data()));
			fs->write(io_buffer.data(), curr_io_request->size);
			if (fs->rdstate() != std::ios_base::goodbit) {
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

void
enqueue_io_packet(uint32_t addr)
{
	packed_io_request packed_curr_io_request;
	std::unique_ptr<io_request> curr_io_request = std::make_unique<io_request>();
	mem_read_block_virt(g_cpu, addr, sizeof(packed_io_request), (uint8_t *)&packed_curr_io_request);
	curr_io_request->id = packed_curr_io_request.id;
	curr_io_request->type = packed_curr_io_request.type;
	curr_io_request->address = packed_curr_io_request.handle_or_address;
	curr_io_request->offset = packed_curr_io_request.offset;
	curr_io_request->size = packed_curr_io_request.size;
	curr_io_request->handle = packed_curr_io_request.handle_or_path;

	if ((IO_GET_TYPE(curr_io_request->type) == open) || (IO_GET_TYPE(curr_io_request->type) == create)) {
		char *path = new char[curr_io_request->size];
		mem_read_block_virt(g_cpu, curr_io_request->handle, curr_io_request->size, (uint8_t *)path);
		curr_io_request->path = path;
	}

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
query_io_packet(uint32_t id, bool query_status)
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
	curr_dir /= "Harddisk/";
	curr_dir.make_preferred();
	if (!::create_directory(curr_dir)) {
		return false;
	}
	for (unsigned i = 1; i < 8; ++i) {
		std::filesystem::path curr_partition_dir = curr_dir / ("Partition" + std::to_string(i));
		curr_partition_dir.make_preferred();
		if (!::create_directory(curr_partition_dir)) {
			return false;
		}
	}

	hdd_path = curr_dir;
	dvd_path = std::filesystem::path(xbe_path).remove_filename();
	xbe_name = std::filesystem::path(xbe_path).filename().string();

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
