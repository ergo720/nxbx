// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "io.hpp"
#include "logger.hpp"
#include "cpu.hpp"
#include "eeprom.hpp"
#include "kernel.hpp"
#include "xiso.hpp"
#include "xpartition.hpp"
#include <thread>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <cinttypes>
#include <cassert>
#include <array>

#define MODULE_NAME io

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
#define CDROM_HANDLE      DEV_CDROM
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

namespace io {
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
		open = 1 << 28,
		remove1 = 2 << 28,
		close = 3 << 28,
		read = 4 << 28,
		write = 5 << 28,
	};

	enum io_flags : uint32_t {
		is_directory = 1 << 3,
		must_be_a_dir = 1 << 4,
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

	enum class dev_t : uint32_t {
		hdd,
		dvd,
	};

	// io_request as used in nboxkrnl, also packed to make sure it has the same padding and alignment
#pragma pack(1)
	struct packed_io_request {
		uint64_t id;
		io_request_type type;
		int64_t offset_or_size;
		uint32_t size;
		uint64_t handle_or_address;
		uint64_t handle_or_path;
	};

	// io_info_block as used in nboxkrnl, also packed to make sure it has the same padding and alignment
	struct io_info_block {
		io_status status;
		io_info info;
		uint64_t info2_or_id; // extra info or id of the io request to query
		uint32_t ready; // set to 0 by the guest, then set to 1 by the host when the io request is complete
	};
#pragma pack()

	// Type layout of io_request
	// io_request_type - dev_type - io_flags - disposition
	// 31 - 28           27 - 23    22 - 3     2 - 0

	// Host version of io_request
	struct io_request {
		~io_request();
		uint64_t id; // unique id to identify this request
		io_request_type type; // type of request and flags
		union {
			int64_t offset; // file offset from which to start the I/O
			uint32_t initial_size; // initial file size for create requests only
		};
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
		io_info_block info; // holds the result of the transfer
		std::unique_ptr<char[]> io_buffer; // holds the data to be transferred (r/w requests only)
	};

	// Info about an opened file
	struct io_file_info {
		std::fstream fs; // opened file stream
		std::string path; // host path of the file
		uint64_t offset; // file offset inside xiso, or zero for everything else
	};

	// Info about a parsed xbox file path
	struct io_path_info {
		std::filesystem::path dev_path;
		std::string remaining_name;
		dev_t dev_type;
	};

	io_request::~io_request()
	{
		io_request_type io_type = IO_GET_TYPE(this->type);
		if (io_type == open) {
			delete[] this->path;
			this->path = nullptr;
		}
	}

	static cpu_t *lc86cpu;
	static std::jthread jthr;
	static std::deque<std::unique_ptr<io_request>> curr_io_queue;
	static std::vector<std::unique_ptr<io_request>> pending_io_vec;
	static std::unordered_map<uint64_t, std::unique_ptr<io_request>> completed_io_info;
	static std::array<std::unordered_map<uint64_t, io_file_info>, NUM_OF_DEVS> xbox_handle_map;
	static std::mutex queue_mtx;
	static std::mutex completed_io_mtx;
	static std::atomic_flag io_pending;

	static std::filesystem::path hdd_path;
	static std::filesystem::path dvd_path;
	static std::filesystem::path eeprom_path;


	static bool
	open_special_files()
	{
		const auto &lambda = [](std::filesystem::path resolved_path, uint64_t handle) -> bool {
			if (auto opt = open_file(resolved_path); !opt) {
				return false;
			}
			else {
				auto pair = xbox_handle_map[handle].emplace(handle, io_file_info{ std::move(*opt), resolved_path.string(), handle == CDROM_HANDLE ? xiso::image_offset : 0 });
				assert(pair.second == true);
				return true;
			}
			};

		if (dvd_input_type == input_t::xiso) {
			if (!lambda((xiso::dvd_image_path).make_preferred(), CDROM_HANDLE)) {
				return false;
			}
		}

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

	static io_path_info
	parse_path(io_request *curr_io_request)
	{
		// NOTE1: Paths from the kernel should have the form "\device\<device name>\<partition number (optional)>\<file name>"
		// "device name" can be CdRom0, Harddisk0 and "partition number" can be Partition0, Partition1, ...
		// NOTE2: path comparisons are case-insensitive in the xbox kernel, so we need to do the same

		std::string_view path(curr_io_request->path, curr_io_request->size);
		size_t dev_pos = path.find_first_of('\\', 1); // discards "device"
		assert(dev_pos != std::string_view::npos);
		size_t pos = std::min(path.find_first_of('\\', dev_pos + 1), path.length() + 1);
		assert(pos != std::string_view::npos);
		util::xbox_string_view device = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(path.substr(dev_pos + 1, pos - dev_pos - 1)); // extracts device name
		std::filesystem::path resolved_path;
		io_path_info path_info;
		if (device.compare("CdRom0") == 0) {
			resolved_path = dvd_path;
			path_info.dev_path = resolved_path;
			path_info.dev_type = dev_t::dvd;
		}
		else {
			resolved_path = hdd_path;
			size_t pos2 = std::min(path.find_first_of('\\', pos + 1), path.length());
			unsigned partition_num = std::strtoul(&path[pos2 - 1], nullptr, 10);
			assert(partition_num < XBOX_NUM_OF_PARTITIONS);
			std::string partition = "Partition" + std::to_string(partition_num); // extracts partition number
			resolved_path /= partition;
			pos = pos2;
			path_info.dev_path = resolved_path;
			path_info.dev_type = dev_t::hdd;
		}
		std::string name(path.substr(std::min(pos + 1, path.length())));
		xbox_to_host_separator(name);
		path_info.remaining_name = name;

		return path_info;
	}

	static void
	worker(std::stop_token stok)
	{
		while (true) {

			// Wait until there's some work to do
			io_pending.wait(false);

			// Check to see if we need to terminate this thread
			if (stok.stop_requested()) [[unlikely]] {
				pending_packets = false;
				curr_io_queue.clear();
				completed_io_info.clear();
				for (auto &handle_map : xbox_handle_map) {
					handle_map.clear();
				}
				pending_io_vec.clear();
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

				auto path_info = parse_path(curr_io_request.get());
				io_info_block io_result(error, no_data, 0);
				uint32_t disposition = IO_GET_DISPOSITION(curr_io_request->type);
				uint32_t flags = IO_GET_FLAGS(curr_io_request->type);
				bool is_directory = flags & io_flags::is_directory;

				const auto add_to_map = [&curr_io_request, dev](auto &&opt, io_info_block *io_result, std::filesystem::path resolved_path, uint64_t file_offset) {
					// NOTE: this insertion will fail when the guest creates a new handle to the same file. This, because it will pass the same host handle, and std::unordered_map
					// doesn't allow duplicated keys. This is ok though, because we can reuse the same std::fstream for the same file and it will have the same path too
					logger_en(info, "Opened %s with handle %" PRIu64 " and path %s", opt->is_open() ? "file" : "directory", curr_io_request->handle_oc, resolved_path.string().c_str());
					auto pair = xbox_handle_map[dev].emplace(curr_io_request->handle_oc, io_file_info{ std::move(*opt), resolved_path.string(), file_offset });
					io_result->status = success;
					};
				const auto check_dir_flags = [flags](bool host_is_directory, io_info_block *io_result) -> bool {
					if ((flags & must_be_a_dir) && !host_is_directory) {
						io_result->status = not_a_directory;
						return false;
					}
					else if ((flags & must_not_be_a_dir) && host_is_directory) {
						io_result->status = is_a_directory;
						return false;
					}
					else {
						return true;
					}
					};

				if ((path_info.dev_type == dev_t::dvd) && (dvd_input_type == input_t::xiso)) {
					xiso::file_info_t file_info = xiso::search_file(path_info.remaining_name);
					if (file_info.exists) {
						assert((disposition == IO_OPEN) || (disposition == IO_OPEN_IF));
						io_result.info = exists;
						if (check_dir_flags(file_info.is_directory, &io_result)) {
							io_result.info = opened;
							if (is_directory) {
								// Open directory
								add_to_map(std::make_optional<std::fstream>(), &io_result, xiso::dvd_image_path, 0);
							}
							else {
								// Open file
								io_result.info2_or_id = file_info.size;
								add_to_map(std::make_optional<std::fstream>(std::move(file_info.fs)), &io_result, xiso::dvd_image_path, file_info.offset);
							}
						}
					}
				}
				else {
					bool host_is_directory;
					std::filesystem::path resolved_path;
					if (file_exists(path_info.dev_path, path_info.remaining_name, resolved_path, &host_is_directory)) {
						io_result.info = exists;
						if (disposition == IO_CREATE) {
							// Create if doesn't exist - FILE_CREATE
							io_result.status = failed;
							io_result.info = exists;
						}
						else if ((disposition == IO_OPEN) || (disposition == IO_OPEN_IF)) {
							// Open if exists - FILE_OPEN
							// Open always - FILE_OPEN_IF
							if (check_dir_flags(host_is_directory, &io_result)) {
								if (is_directory) {
									// Open directory: nothing to do
									io_result.info = opened;
									add_to_map(std::make_optional<std::fstream>(), &io_result, resolved_path, 0);
								}
								else {
									// Open file
									if (auto opt = open_file(resolved_path, &io_result.info2_or_id); opt) {
										io_result.info = opened;
										add_to_map(opt, &io_result, resolved_path, 0);
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
									io_result.info = exists;
									add_to_map(std::make_optional<std::fstream>(), &io_result, resolved_path, 0);
								}
								else {
									// Create file
									if (auto opt = create_file(resolved_path, curr_io_request->initial_size); opt) {
										io_result.info = (disposition == IO_SUPERSEDE) ? superseded : overwritten;
										add_to_map(opt, &io_result, resolved_path, 0);
									}
								}
							}
						}
					}
					else {
						io_result.info = not_exists;
						if ((disposition == IO_CREATE) || (disposition == IO_SUPERSEDE) || (disposition == IO_OPEN_IF) || (disposition == IO_OVERWRITE_IF)) {
							// Create if doesn't exist - FILE_CREATE
							// Create always - FILE_SUPERSEDE
							// Open always - FILE_OPEN_IF
							// Truncate always - FILE_OVERWRITE_IF
							if (is_directory) {
								// Create directory
								if (::create_directory(resolved_path)) {
									io_result.info = created;
									add_to_map(std::make_optional<std::fstream>(), &io_result, resolved_path, 0);
								}
							}
							else {
								// Create file
								if (auto opt = create_file(resolved_path, curr_io_request->initial_size); opt) {
									io_result.info = created;
									add_to_map(opt, &io_result, resolved_path, 0);
								}
							}
						}
						else {
							// Open if exists - FILE_OPEN
							// Truncate if exists - FILE_OVERWRITE
							io_result.status = not_found;
							io_result.info = not_exists;
						}
					}
				}

				curr_io_request->info = io_result;
				completed_io_mtx.lock();
				completed_io_info.emplace(curr_io_request->id, std::move(curr_io_request));
				completed_io_mtx.unlock();
				continue;
			}

			io_info_block io_result(success, no_data);
			auto it = xbox_handle_map[dev].find(curr_io_request->handle);
			if (it == xbox_handle_map[dev].end()) [[unlikely]] {
				logger_en(warn, "Handle %" PRIu64 " not found", curr_io_request->handle); // this should not happen...
				io_result.status = error;
				completed_io_mtx.lock();
				completed_io_info.emplace(curr_io_request->id, std::move(curr_io_request));
				completed_io_mtx.unlock();
				continue;
			}

			std::fstream *fs = &it->second.fs;
			switch (io_type)
			{
			case io_request_type::close:
				logger_en(info, "Closed file handle %" PRIu64 " with path %s", it->first, it->second.path.c_str());
				xbox_handle_map[dev].erase(it);
				break;

			case io_request_type::read:
				if (!fs->is_open()) [[unlikely]] {
					// Read operation on a directory (this should not happen...)
					logger_en(warn, "Read operation to directory handle %" PRIu64 " with path %s", it->first, it->second.path.c_str());
					io_result.status = error;
					io_result.info = no_data;
					break;
				}
				curr_io_request->io_buffer = std::unique_ptr<char[]>(new char[curr_io_request->size]);
				fs->seekg(curr_io_request->offset + it->second.offset);
				fs->read(curr_io_request->io_buffer.get(), curr_io_request->size);
				if (fs->good()) {
					io_result.info = static_cast<io_info>(fs->gcount());
					logger_en(info, "Read operation to file handle %" PRIu64 ", offset=0x%08" PRIX32 ", size=0x%08" PRIX32 ", actual bytes transferred=0x%08" PRIX32 " -> OK!",
						it->first, curr_io_request->offset, curr_io_request->size, io_result.info);
				}
				else {
					io_result.status = error;
					fs->clear();
					logger_en(info, "Read operation to file handle %" PRIu64 " with path %s, offset=0x%08" PRIX32 ", size=0x%08" PRIX32 " -> FAILED!",
						it->first, it->second.path.c_str(), curr_io_request->offset, curr_io_request->size);
				}
				break;

			case io_request_type::write:
				if (!fs->is_open()) [[unlikely]] {
					// Write operation on a directory (this should not happen...)
					logger_en(warn, "Write operation to directory handle %" PRIu64 " with path %s", it->first, it->second.path.c_str());
					io_result.status = error;
					io_result.info = no_data;
					break;
				}
				fs->seekg(curr_io_request->offset + it->second.offset);
				fs->write(curr_io_request->io_buffer.get(), curr_io_request->size);
				if (!fs->good()) {
					io_result.status = error;
					io_result.info = no_data;
					fs->clear();
					logger_en(info, "Write operation to file handle %" PRIu64 " with path %s, offset=0x%08" PRIX32 ", size=0x%08" PRIX32 " -> FAILED!",
						it->first, it->second.path.c_str(), curr_io_request->offset, curr_io_request->size);
				}
				else {
					io_result.info = static_cast<io_info>(curr_io_request->size);
					logger_en(info, "Write operation to file handle %" PRIu64 ", offset=0x%08" PRIX32 ", size=0x%08" PRIX32 " -> OK!",
						it->first, curr_io_request->offset, curr_io_request->size);
				}
				break;

			case io_request_type::remove1: {
				logger_en(info, "Deleted %s with handle %" PRIu64, fs->is_open() ? "file" : "directory", it->first);
				std::string file_path(it->second.path);
				xbox_handle_map[dev].erase(it);
				std::error_code ec;
				std::filesystem::remove(file_path, ec);
			}
			break;

			default:
				logger_en(warn, "Unknown io request of type %" PRId32, curr_io_request->type);
			}

			curr_io_request->info = io_result;
			completed_io_mtx.lock();
			completed_io_info.emplace(curr_io_request->id, std::move(curr_io_request));
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
		mem_read_block_virt(lc86cpu, addr, sizeof(packed_io_request), (uint8_t *)&packed_curr_io_request);
		curr_io_request->id = packed_curr_io_request.id;
		curr_io_request->type = packed_curr_io_request.type;
		curr_io_request->handle_oc = packed_curr_io_request.handle_or_address;
		curr_io_request->offset = packed_curr_io_request.offset_or_size;
		curr_io_request->size = packed_curr_io_request.size;
		curr_io_request->handle = packed_curr_io_request.handle_or_path;

		io_request_type io_type = IO_GET_TYPE(curr_io_request->type);
		if (io_type == open) {
			char *path = new char[curr_io_request->size + 1];
			mem_read_block_virt(lc86cpu, (addr_t)curr_io_request->handle, curr_io_request->size, (uint8_t *)path);
			curr_io_request->path = path;
			curr_io_request->path[curr_io_request->size] = '\0';
		}
		else if (io_type == write) {
			std::unique_ptr<char[]> io_buffer(new char[curr_io_request->size]);
			mem_read_block_virt(lc86cpu, curr_io_request->address, curr_io_request->size, reinterpret_cast<uint8_t *>(io_buffer.get()));
			curr_io_request->io_buffer = std::move(io_buffer);
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

	void
	query_io_packet(uint32_t addr)
	{
		if (completed_io_mtx.try_lock()) { // don't wait if the I/O thread is currently using the map
			io_info_block block;
			mem_read_block_virt(lc86cpu, addr, sizeof(io_info_block), (uint8_t *)&block);
			auto it = completed_io_info.find(block.info2_or_id);
			if (it != completed_io_info.end()) {
				io_request *request = it->second.get();
				if ((IO_GET_TYPE(request->type) == read) && (request->info.status == success)) {
					mem_write_block_virt(lc86cpu, request->address, request->size, request->io_buffer.get());
				}
				block = request->info;
				block.ready = 1;
				mem_write_block_virt(lc86cpu, addr, sizeof(io_info_block), (uint8_t *)&block);
				completed_io_info.erase(it);
			}
			completed_io_mtx.unlock();
		}
	}

	bool
	init(const init_info_t &init_info, cpu_t *cpu)
	{
		lc86cpu = cpu;
		std::filesystem::path curr_dir = init_info.m_nxbx_path;
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
				logger_en(error, "Failed to create partition metadata bin files");
				return false;
			}
		}
		std::filesystem::path eeprom_dir = curr_dir;
		eeprom_dir /= "eeprom.bin";
		eeprom_dir.make_preferred();
		if (!file_exists(eeprom_dir)) {
			if (auto opt = create_file(eeprom_dir); !opt) {
				logger_en(error, "Failed to create eeprom file");
				return false;
			}
			else {
				if (!gen_eeprom(std::move(opt.value()))) {
					logger_en(error, "Failed to update eeprom file");
					return false;
				}
			}
		}

		if (init_info.m_input_type == input_t::xiso) {
			xbe_name = "default.xbe";
			hdd_path = hdd_dir;
			dvd_path = std::filesystem::path(init_info.m_input_path).make_preferred().remove_filename();
			eeprom_path = eeprom_dir.remove_filename();
			xbe_path = "\\Device\\CdRom0\\" + xbe_name;
			dvd_input_type = input_t::xiso;
		}
		else {
			std::filesystem::path local_xbe_path = std::filesystem::path(init_info.m_input_path).make_preferred();
			xbe_name = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(local_xbe_path.filename().string());
			hdd_path = hdd_dir;
			dvd_path = local_xbe_path.remove_filename();
			eeprom_path = eeprom_dir.remove_filename();
			xbe_path = "\\Device\\CdRom0\\" + xbe_name;
			dvd_input_type = input_t::xbe;
			if (dvd_path.string().starts_with(hdd_path.string())) {
				// XBE is installed inside a HDD partition, so set the dvd drive to be empty by setting th dvd path to an invalid directory
				// TODO: this should also set the SMC tray state to have no media
				size_t partition_num_off = hdd_path.string().size() + 9;
				std::string xbox_hdd_dir = "\\Device\\Harddisk0\\Partition" + std::to_string(dvd_path.string()[partition_num_off] - '0');
				std::string xbox_remaining_hdd_dir = dvd_path.string().substr(partition_num_off + 1);
				for (size_t pos = 0; pos < xbox_remaining_hdd_dir.size(); ++pos) {
					if (xbox_remaining_hdd_dir[pos] == '/') {
						xbox_remaining_hdd_dir[pos] = '\\'; // convert to xbox path separator
					}
				}
				xbe_path = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(xbox_hdd_dir + xbox_remaining_hdd_dir + xbe_name.c_str());
				dvd_path = "";
			}
		}

		if (!open_special_files()) {
			logger_en(error, "Failed to open partition metadata bin files");
			return false;
		}

		jthr = std::jthread(&io::worker);

		return true;
	}

	void
	stop()
	{
		if (jthr.joinable()) {
			// Signal the I/O thread that it needs to exit
			jthr.request_stop();
			queue_mtx.lock();
			io_pending.test_and_set();
			io_pending.notify_one();
			queue_mtx.unlock();
			jthr.join();
		}
	}
}
