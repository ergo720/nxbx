// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2023 ergo720

#include "io.hpp"
#include "logger.hpp"
#include "cpu.hpp"
#include "kernel.hpp"
#include "xdvdfs.hpp"
#include "fatx.hpp"
#include "console.hpp"
#include <thread>
#include <deque>
#include <map>
#include <mutex>
#include <filesystem>
#include <cinttypes>
#include <cassert>
#include <array>

#define MODULE_NAME io

// Disposition flags (same as used by NtCreate/OpenFile)
#define IO_SUPERSEDE    0
#define IO_OPEN         1
#define IO_CREATE       2
#define IO_OPEN_IF      3
#define IO_OVERWRITE    4
#define IO_OVERWRITE_IF 5

#define IO_GET_TYPE(type) (request_type_t)((uint32_t)(type) & 0xF0000000)
#define IO_GET_FLAGS(type) ((uint32_t)(type) & 0x007FFFF8)
#define IO_GET_DISPOSITION(type) ((uint32_t)(type) & 0x00000007)
#define IO_GET_DEV(type) (((uint32_t)(type) >> 23) & 0x0000001F)

namespace io {
	// These definitions are the same used by nboxkrnl to submit I/O request, and should be kept synchronized with those
	enum request_type_t : uint32_t {
		open = 1 << 28,
		remove = 2 << 28,
		close = 3 << 28,
		read = 4 << 28,
		write = 5 << 28,
	};

	enum info_t : uint32_t {
		no_data = 0,
		superseded = 0,
		opened,
		created,
		overwritten,
		exists,
		not_exists
	};

#pragma pack(1)
	// io request packet and header as used in nboxkrnl, also packed to make sure it has the same padding and alignment
	struct packed_request_header_t {
		uint32_t id; // unique id to identify this request
		uint32_t type; // type of request and flags
	};

	// Generic i/o request from the guest
	struct packed_request_xx_t {
		uint32_t handle; // file handle
	};

	// Specialized version of packed_request_xx_t for read/write requests only
	struct packed_request_rw_t {
		int64_t offset; // file offset from which to start the I/O
		uint32_t size; // bytes to transfer
		uint32_t address; // virtual address of the data to transfer
		uint32_t handle; // file handle
		uint32_t timestamp; // fatx timestamp
	};

	// Specialized version of packed_request_xx_t for open/create requests only
	struct packed_request_oc_t {
		int64_t initial_size; // file initial size
		uint32_t size; // size of file path
		uint32_t handle; // file handle
		uint32_t path; // file path address
		uint32_t attributes; // file attributes (only uses a single byte really)
		uint32_t timestamp; // file timestamp
		uint32_t desired_access; // the kind of access requested for the file
		uint32_t create_options; // how the create the file
	};

	struct packed_request_t {
		packed_request_header_t header;
		union {
			packed_request_oc_t m_oc;
			packed_request_rw_t m_rw;
			packed_request_xx_t m_xx;
		};
	};

	// io_info_block as used in nboxkrnl, also packed to make sure it has the same padding and alignment
	struct info_block_t {
		uint32_t id; // unique id to identify this request (it's the address of this io request itself)
		status_t status; // the final status of the request
		info_t info; // request-specific information
		uint32_t ready; // set to 0 by the guest, then set to 1 by the host when the io request is complete
	};

	// Specialized version of info_block_t for open/create requests only
	struct info_block_oc_t {
		info_block_t header;
		uint32_t file_size; // actual size of the opened file
		union {
			struct {
				uint32_t free_clusters; // number of free clusters left
				uint32_t creation_time;
				uint32_t last_access_time;
				uint32_t last_write_time;
			} fatx;
			int64_t xdvdfs_timestamp;
		};
	};
#pragma pack()

	static_assert(std::is_trivially_copyable_v<packed_request_t>);
	static_assert(std::is_trivially_copyable_v<info_block_t>);
	static_assert(sizeof(packed_request_t) == 44);
	static_assert(sizeof(info_block_oc_t) == 36);

	// Type layout of io_request
	// io_request_type - dev_type - io_flags - disposition
	// 31 - 28           27 - 23    22 - 3     2 - 0

	// Host version of io_request
	struct request_t {
		~request_t();
		uint32_t id; // unique id to identify this request
		uint32_t type; // type of request and flags
		union {
			int64_t offset; // file offset from which to start the I/O
			uint32_t initial_size; // initial file size for create requests only
		};
		union {
			// virtual address of the data to transfer or file path for open/create requests
			uint32_t address;
			char *path;
		};
		uint32_t size; // bytes to transfer or size of path for open/create requests
		uint32_t handle; // file handle
		uint32_t timestamp; // file timestamp
		info_block_oc_t info; // holds the result of the transfer
	};

	struct request_oc_t : public request_t {
		uint32_t attributes; // file attributes (only uses a single byte really)
		uint32_t desired_access; // the kind of access requested for the file
		uint32_t create_options; // how the create the file
	};

	struct request_rw_t : public request_t {
		std::unique_ptr<char[]> buffer; // holds the data to be transferred
	};

	// Basic info about an opened file
	struct file_info_base_t {
		file_info_base_t(std::fstream &&f, std::string p) : fs(std::move(f)), path(p) {};
		std::fstream fs; // fs of a file
		std::string path; // same relative path returned by io::parse_path()
	};

	// file_info_base_t that holds additional info about a fatx file
	struct file_info_fatx_t : public file_info_base_t {
		file_info_fatx_t(std::fstream &&f, std::string p, uint64_t o, fatx::DIRENT d) : file_info_base_t(std::move(f), p), dirent_offset(o), dirent(d) {};
		uint64_t dirent_offset; // dirent offset in metadata.bin
		fatx::DIRENT dirent; // a cached copy of the dirent
		void last_access_time(uint32_t time) { dirent.last_access_time = time; };
		void last_write_time(uint32_t time) { dirent.last_write_time = time; };
		void set_dirent(fatx::DIRENT &dir) { dirent = dir; };
	};

	// file_info_base_t that holds additional info about a xdvdfs file
	struct file_info_xdvdfs_t : public file_info_base_t  {
		file_info_xdvdfs_t(std::fstream &&f, std::string p, uint64_t o) : file_info_base_t(std::move(f), p), offset(o) {};
		uint64_t offset; // offset of the file inside the xiso image
	};

	request_t::~request_t()
	{
		request_type_t io_type = IO_GET_TYPE(this->type);
		if (io_type == open) {
			delete[] this->path;
			this->path = nullptr;
		}
	}

	static cpu_t *s_lc86cpu;
	static std::jthread s_jthr;
	static std::deque<std::unique_ptr<request_t>> s_curr_io_queue;
	static std::vector<std::unique_ptr<request_t>> s_pending_io_vec;
	static std::unordered_map<uint32_t, std::unique_ptr<request_t>> s_completed_io_info;
	static std::array<std::map<uint32_t, std::unique_ptr<file_info_base_t>>, NUM_OF_DEVS> s_xbox_handle_map;
	static std::mutex s_queue_mtx;
	static std::mutex s_completed_io_mtx;
	static std::atomic_flag s_pending_io;
	static input_t s_dvd_input_type;


	static void
	flush_all_files()
	{
		for (unsigned i = DEV_PARTITION1; i < DEV_PARTITION6; ++i) {
			s_xbox_handle_map[i].erase(s_xbox_handle_map[i].begin()); // delete the partition file object
			std::for_each(s_xbox_handle_map[i].begin(), s_xbox_handle_map[i].end(), [i](auto &pair)
				{
					file_info_fatx_t *file_info_fatx = (file_info_fatx_t *)(pair.second.get());
					if (file_info_fatx->dirent.name[0] != '\\') { // the root directory hasn't a dirent to flush
						fatx::driver::get(i).flush_dirent_for_file(file_info_fatx->dirent, file_info_fatx->dirent_offset);
					}
				});
		}
	}

	static void
	add_device_handles()
	{
		const auto &lambda = [](std::filesystem::path resolved_path, uint32_t handle) {
			auto pair = s_xbox_handle_map[handle].emplace(handle, std::move(std::make_unique<file_info_base_t>(std::move(std::fstream()), resolved_path.string())));
			assert(pair.second == true);
			};

		if (s_dvd_input_type == input_t::xiso) {
			lambda(to_slash_separator(g_dvd_dir / xdvdfs::driver::get().m_xiso_name), CDROM_HANDLE);
		}

		for (unsigned i = 0; i < XBOX_NUM_OF_HDD_PARTITIONS; ++i) {
			std::filesystem::path curr_partition_dir = to_slash_separator(g_hdd_dir / ("Partition" + std::to_string(i) + ".bin"));
			lambda(curr_partition_dir, PARTITION0_HANDLE + i);
		}
	}

	static std::string
	parse_path(request_oc_t *host_io_request)
	{
		// This function takes an xbox path and converts it to a host path relative to the root folder of the device that contains the xbox file/directory
		// NOTE1: Paths from the kernel should have the form "\device\<device name>\<partition number (optional)>\<file name>"
		// "device name" can be CdRom0, Harddisk0 and "partition number" can be Partition0, Partition1, ...
		// NOTE2: path comparisons are case-insensitive in the xbox kernel, so we need to do the same

		std::string_view path(host_io_request->path, host_io_request->size);
		size_t dev_pos = path.find_first_of('\\', 1); // discards "device"
		assert(dev_pos != std::string_view::npos);
		size_t pos = std::min(path.find_first_of('\\', dev_pos + 1), path.length() + 1);
		assert(pos != std::string_view::npos);
		util::xbox_string_view device = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(path.substr(dev_pos + 1, pos - dev_pos - 1)); // extracts device name
		std::filesystem::path resolved_path;

		if (device.compare("CdRom0") == 0) {
			resolved_path = "";
		}
		else {
			resolved_path = "Harddisk";
			size_t pos2 = std::min(path.find_first_of('\\', pos + 1), path.length());
			unsigned partition_num = std::strtoul(&path[pos2 - 1], nullptr, 10);
			assert(partition_num < XBOX_NUM_OF_HDD_PARTITIONS);
			std::string partition = "Partition" + std::to_string(partition_num); // extracts partition number
			resolved_path /= partition;
			pos = pos2;
		}
		std::string name(path.substr(std::min(pos + 1, path.length())));
		resolved_path /= name;
		std::string resolved_str(resolved_path.string());
		std::replace(resolved_str.begin(), resolved_str.end(), '\\', '/'); // xbox paths always use the backslash

		return resolved_str;
	}

	static void
	worker(std::stop_token stok)
	{
		while (true) {

			// Wait until there's some work to do
			s_pending_io.wait(false);

			// Check to see if we need to terminate this thread
			if (stok.stop_requested()) [[unlikely]] {
				flush_all_files();
				fatx::driver::deinit();
				pending_packets = false;
				s_curr_io_queue.clear();
				s_completed_io_info.clear();
				for (auto &handle_map : s_xbox_handle_map) {
					handle_map.clear();
				}
				s_pending_io_vec.clear();
				return;
			}

			s_queue_mtx.lock();
			if (s_curr_io_queue.empty()) {
				s_pending_io.clear();
				s_queue_mtx.unlock();
				continue;
			}
			std::unique_ptr<request_t> host_io_request = std::move(s_curr_io_queue.front());
			s_curr_io_queue.pop_front();
			s_queue_mtx.unlock();

			request_type_t io_type = IO_GET_TYPE(host_io_request->type);
			uint32_t dev = IO_GET_DEV(host_io_request->type);
			if (io_type == open) {
				// This code opens/creates the file according to the CreateDisposition parameter used by NtCreate/OpenFile

				request_oc_t *curr_oc_request = (request_oc_t *)host_io_request.get();
				std::string relative_path = parse_path(curr_oc_request);
				info_block_oc_t io_result;
				std::fill_n((char *)&io_result.header, sizeof(io_result.header), 0);
				io_result.header.status = STATUS_IO_DEVICE_ERROR;
				uint32_t disposition = IO_GET_DISPOSITION(host_io_request->type);
				uint32_t flags = IO_GET_FLAGS(host_io_request->type);

				if (dev == DEV_CDROM) {
					xdvdfs::file_info_t file_info;
					std::optional<std::fstream> opt = std::fstream();
					if (s_dvd_input_type == input_t::xiso) {
						file_info = xdvdfs::driver::get().search_file(relative_path); // search for the file in the xiso
					} else {
						assert(s_dvd_input_type == input_t::xbe);
						std::filesystem::path resolved_path;
						file_info.exists = file_exists(g_dvd_dir, relative_path, resolved_path, &file_info.is_directory); // search for the file in the dvd folder
						if (file_info.exists) {
							file_info.offset = file_info.size = file_info.timestamp = 0;
							if (!file_info.is_directory) {
								if (opt = open_file(resolved_path, &file_info.size); opt) {
									std::error_code ec;
									file_info.size = std::filesystem::file_size(resolved_path, ec);
									if (ec) {
										file_info.exists = false; // make the request fail
									}
								}
							}
						}
					}

					if (file_info.exists) {
						assert((disposition == IO_OPEN) || (disposition == IO_OPEN_IF));
						io_result.header.info = exists;

						if ((flags & io::flags_t::must_be_a_dir) && !file_info.is_directory) {
							io_result.header.status = STATUS_NOT_A_DIRECTORY;
						}
						else if ((flags & io::flags_t::must_not_be_a_dir) && file_info.is_directory) {
							io_result.header.status = STATUS_FILE_IS_A_DIRECTORY;
						}
						else {
							io_result.header.status = STATUS_SUCCESS;
							io_result.header.info = opened;
							io_result.file_size = file_info.size;
							io_result.xdvdfs_timestamp = file_info.timestamp;
							s_xbox_handle_map[dev].emplace(curr_oc_request->handle, std::move(std::make_unique<file_info_xdvdfs_t>(std::move(*opt), relative_path, file_info.offset)));
							logger_en(info, "Opened %s with handle 0x%08" PRIX32 " and path %s", file_info.is_directory ? "directory" : "file", curr_oc_request->handle, relative_path.c_str());
						}
					}
				}
				else {
					const auto add_to_map = [curr_oc_request, dev, &relative_path](auto &&opt, info_block_oc_t *io_result, uint64_t dirent_offset, fatx::DIRENT &io_dirent) {
							// NOTE: this insertion will fail when the guest creates a new handle to the same file. This, because it will pass the same host handle, and std::unordered_map
							// doesn't allow duplicated keys. This is ok though, because we can reuse the same std::fstream for the same file and it will have the same path too
							logger_en(info, "Opened %s with handle 0x%08" PRIX32 " and path %s", opt->is_open() ? "file" : "directory", curr_oc_request->handle, relative_path.c_str());
							s_xbox_handle_map[dev].emplace(curr_oc_request->handle, std::move(std::make_unique<file_info_fatx_t>(std::move(*opt), relative_path, dirent_offset, io_dirent)));
							io_result->header.status = STATUS_SUCCESS;
							io_result->file_size = io_dirent.size;
							io_result->fatx.creation_time = io_dirent.creation_time;
							io_result->fatx.last_access_time = io_dirent.last_access_time;
							io_result->fatx.last_write_time = io_dirent.last_write_time;
							io_result->fatx.free_clusters = fatx::driver::get(dev).get_free_cluster_num();
						};

					fatx::DIRENT io_dirent;
					std::filesystem::path resolved_path;
					uint64_t dirent_offset;
					status_t fatx_search_status = fatx::driver::get(dev).find_dirent_for_file(relative_path, io_dirent, dirent_offset);

					if (fatx_search_status == IS_ROOT_DIRECTORY) {
						assert((disposition == IO_OPEN) || (disposition == IO_OPEN_IF));

						io_dirent.name_length = 1;
						io_dirent.attributes = IO_FILE_DIRECTORY;
						io_dirent.name[0] = '\\';
						io_dirent.first_cluster = 1;
						io_dirent.size = 0;
						io_dirent.creation_time = curr_oc_request->timestamp;
						io_dirent.last_write_time = curr_oc_request->timestamp;
						io_dirent.last_access_time = curr_oc_request->timestamp;

						io_result.header.info = opened;
						add_to_map(std::make_optional<std::fstream>(), &io_result, 0, io_dirent);
					}
					else if (fatx_search_status == STATUS_SUCCESS) {
						if (!file_exists(g_nxbx_dir, relative_path, resolved_path)) {
							// The fatx fs indicates that the file exists, but it doesn't on the host side. This can happen for example if the user manually moves/deletes
							// the host file with the OS
							logger_en(error, "File with path %s exists on fatx but doesn't on the host", relative_path.c_str());
						} else {
							bool is_directory = io_dirent.attributes & IO_FILE_DIRECTORY;
							io_result.header.info = exists;
							if (disposition == IO_CREATE) {
								// Create if doesn't exist - FILE_CREATE
								io_result.header.status = STATUS_ACCESS_DENIED;
								io_result.header.info = exists;
							} else if ((disposition == IO_OPEN) || (disposition == IO_OPEN_IF)) {
								// Open if exists - FILE_OPEN
								// Open always - FILE_OPEN_IF
								status_t status = fatx::driver::get(dev).check_file_access(curr_oc_request->desired_access, curr_oc_request->create_options, io_dirent.attributes, false, flags);
								if (status == STATUS_SUCCESS) {
									if (is_directory) {
										// Open directory: nothing to do
										io_result.header.info = opened;
										add_to_map(std::make_optional<std::fstream>(), &io_result, dirent_offset, io_dirent);
									} else {
										// Open file
										if (auto opt = open_file(resolved_path); opt) {
											io_result.header.info = opened;
											add_to_map(opt, &io_result, dirent_offset, io_dirent);
										}
									}
								}
							} else {
								// Create always - FILE_SUPERSEDE
								// Truncate if exists - FILE_OVERWRITE
								// Truncate always - FILE_OVERWRITE_IF
								status_t status = fatx::driver::get(dev).check_file_access(curr_oc_request->desired_access, curr_oc_request->create_options, io_dirent.attributes, true, flags);
								if (status == STATUS_SUCCESS) {
									io_dirent.attributes = curr_oc_request->attributes;
									io_dirent.last_write_time = curr_oc_request->timestamp;
									if (is_directory) {
										// Create directory: already exists
										if (fatx::driver::get(dev).overwrite_dirent_for_file(io_dirent, 0, "") == STATUS_SUCCESS) {
											io_result.header.info = exists;
											add_to_map(std::make_optional<std::fstream>(), &io_result, dirent_offset, io_dirent);
										}
									} else {
										// Create file
										if (auto opt = create_file(resolved_path, curr_oc_request->initial_size); opt) {
											if (fatx::driver::get(dev).overwrite_dirent_for_file(io_dirent, curr_oc_request->initial_size, relative_path) == STATUS_SUCCESS) {
												io_result.header.info = (disposition == IO_SUPERSEDE) ? superseded : overwritten;
												add_to_map(opt, &io_result, dirent_offset, io_dirent);
											}
										}
									}
								}
							}
						}
					}
					else if (fatx_search_status == STATUS_OBJECT_NAME_NOT_FOUND) {
						bool is_directory = curr_oc_request->attributes & IO_FILE_DIRECTORY;
						io_result.header.info = not_exists;
						resolved_path = to_slash_separator(g_nxbx_dir / relative_path);

						// Extract the filename to put in the dirent
						size_t path_length = relative_path.length();
						size_t pos = relative_path[path_length - 1] == '/' ? path_length - 2 : path_length - 1;
						size_t pos2 = relative_path.find_last_of('/', pos);
						assert(pos != std::string::npos);
						std::string file_name(relative_path.substr(pos2 + 1, pos - pos2 + 1));

						if ((disposition == IO_CREATE) || (disposition == IO_SUPERSEDE) || (disposition == IO_OPEN_IF) || (disposition == IO_OVERWRITE_IF)) {
							// Create if doesn't exist - FILE_CREATE
							// Create always - FILE_SUPERSEDE
							// Open always - FILE_OPEN_IF
							// Truncate always - FILE_OVERWRITE_IF
							status_t status = fatx::driver::get(dev).check_file_access(curr_oc_request->desired_access, curr_oc_request->create_options, curr_oc_request->attributes, true, flags);
							if (status == STATUS_SUCCESS) {
								io_dirent.name_length = file_name.length();
								io_dirent.attributes = curr_oc_request->attributes;
								std::copy_n(file_name.c_str(), file_name.length(), io_dirent.name);
								io_dirent.first_cluster = 0; // replaced by create_dirent_for_file()
								io_dirent.creation_time = curr_oc_request->timestamp;
								io_dirent.last_write_time = curr_oc_request->timestamp;
								io_dirent.last_access_time = curr_oc_request->timestamp;
								if (is_directory) {
									// Create directory
									if (::create_directory(resolved_path)) {
										io_dirent.size = 0;
										if (fatx::driver::get(dev).create_dirent_for_file(io_dirent, relative_path) == status_t::STATUS_SUCCESS) {
											io_result.header.info = created;
											add_to_map(std::make_optional<std::fstream>(), &io_result, dirent_offset, io_dirent);
										}
									}
								} else {
									// Create file
									if (auto opt = create_file(resolved_path, curr_oc_request->initial_size); opt) {
										io_dirent.size = curr_oc_request->initial_size;
										if (fatx::driver::get(dev).create_dirent_for_file(io_dirent, relative_path) == status_t::STATUS_SUCCESS) {
											io_result.header.info = created;
											add_to_map(opt, &io_result, dirent_offset, io_dirent);
										}
									}
								}
							}
						} else {
							// Open if exists - FILE_OPEN
							// Truncate if exists - FILE_OVERWRITE
							io_result.header.status = STATUS_OBJECT_NAME_NOT_FOUND;
							io_result.header.info = not_exists;
						}
					}
					else {
						assert((fatx_search_status == STATUS_FILE_CORRUPT_ERROR) || // dirent stream is full or it has an invalid cluster number
							(fatx_search_status == STATUS_IO_DEVICE_ERROR) || // host i/o error
							(fatx_search_status == STATUS_OBJECT_PATH_NOT_FOUND)); // a directory in the middle of the path doesn't exist
						io_result.header.status = fatx_search_status;
					}
				}

				curr_oc_request->info = io_result;
				s_completed_io_mtx.lock();
				s_completed_io_info.emplace(curr_oc_request->id, std::move(host_io_request));
				s_completed_io_mtx.unlock();
				continue;
			}

			info_block_t io_result;
			std::fill_n((char *)&io_result, sizeof(io_result), 0);
			auto it = s_xbox_handle_map[dev].find(host_io_request->handle);
			if (it == s_xbox_handle_map[dev].end()) [[unlikely]] {
				logger_en(warn, "Handle 0x%08" PRIX32 " not found", host_io_request->handle); // this should not happen...
				io_result.status = STATUS_IO_DEVICE_ERROR;
				s_completed_io_mtx.lock();
				s_completed_io_info.emplace(host_io_request->id, std::move(host_io_request));
				s_completed_io_mtx.unlock();
				continue;
			}

			std::fstream *fs = &it->second->fs;
			switch (io_type)
			{
			case request_type_t::close:
				if (dev != DEV_CDROM) {
					file_info_fatx_t *file_info_fatx = (file_info_fatx_t *)(it->second.get());
					if (file_info_fatx->dirent.name[0] != '\\') { // the root directory hasn't a dirent to flush
						fatx::driver::get(dev).flush_dirent_for_file(file_info_fatx->dirent, file_info_fatx->dirent_offset);
					}
				}
				logger_en(info, "Closed file handle 0x%08" PRIX32 " with path %s", it->first, it->second->path.c_str());
				s_xbox_handle_map[dev].erase(it);
				break;

			case request_type_t::read: {
				io_result.status = STATUS_IO_DEVICE_ERROR;
				io_result.info = no_data;

				request_rw_t *curr_rw_request = (request_rw_t *)host_io_request.get();
				if (IS_DEV_HANDLE(curr_rw_request->handle)) {
					if (curr_rw_request->handle == CDROM_HANDLE) {
						if (s_dvd_input_type != input_t::xiso) {
							// We can only handle raw disc accesses if the user has booted from an xiso
							logger_en(error, "Unhandled raw dvd disc read, boot from an xiso to solve this; offset=0x%016" PRIX64 ", size=0x%08" PRIX32,
								curr_rw_request->offset, curr_rw_request->size);
						}
						io_result.status = xdvdfs::driver::get().read_raw_disc(curr_rw_request->offset, curr_rw_request->size, curr_rw_request->buffer.get());
						if (io_result.status == STATUS_SUCCESS) {
							io_result.info = static_cast<info_t>(curr_rw_request->size);
						}
					} else {
						uint64_t offset = curr_rw_request->offset;
						if (curr_rw_request->handle == PARTITION0_HANDLE) {
							// Partition zero can access the whole disk, so figure out the target offset first
							offset = disk_offset_to_partition_offset(curr_rw_request->offset, dev);
						}
						io_result.status = fatx::driver::get(dev).read_raw_partition(offset, curr_rw_request->size, curr_rw_request->buffer.get());
						if (io_result.status == STATUS_SUCCESS) {
							io_result.info = static_cast<info_t>(curr_rw_request->size);
						}
					}
				}
				else {
					uint64_t offset = 0;
					if ((dev == DEV_CDROM) && (s_dvd_input_type == input_t::xiso)) {
						fs = &xdvdfs::driver::get().m_xiso_fs;
						offset = xdvdfs::driver::get().m_xiso_offset + static_cast<file_info_xdvdfs_t &&>(*it->second).offset;
					}
					if (!fs->is_open()) [[unlikely]] {
						// Read operation on a directory (this should not happen...)
						logger_en(warn, "Read operation to directory handle 0x%08" PRIX32 " with path %s", it->first, it->second->path.c_str());
						break;
					}
					fs->seekg(curr_rw_request->offset + offset);
					fs->read(curr_rw_request->buffer.get(), curr_rw_request->size);
					if (fs->good() || fs->eof()) {
						if (dev != DEV_CDROM) {
							static_cast<file_info_fatx_t &&>(*it->second).last_access_time(curr_rw_request->timestamp);
						}
						io_result.status = STATUS_SUCCESS;
						io_result.info = static_cast<info_t>(fs->gcount());
						logger_en(info, "Read operation to file handle 0x%08" PRIX32 ", offset=0x%016" PRIX64 ", size=0x%08" PRIX32 ", actual bytes transferred=0x%08" PRIX32 " -> %s",
							it->first, curr_rw_request->offset, curr_rw_request->size, io_result.info, fs->good() ? "OK!" : "EOF!");
					} else {
						logger_en(info, "Read operation to file handle 0x%08" PRIX32 " with path %s, offset=0x%016" PRIX64 ", size=0x%08" PRIX32 " -> FAILED!",
							it->first, it->second->path.c_str(), curr_rw_request->offset, curr_rw_request->size);
					}
					fs->clear();
				}
			}
			break;

			case request_type_t::write: {
				io_result.status = STATUS_IO_DEVICE_ERROR;
				io_result.info = no_data;

				request_rw_t *curr_rw_request = (request_rw_t *)host_io_request.get();
				if (IS_DEV_HANDLE(curr_rw_request->handle)) {
					if (curr_rw_request->handle == CDROM_HANDLE) [[unlikely]] {
						// Raw write to the dvd disc (this should not happen...)
						io_result.status = STATUS_IO_DEVICE_ERROR;
						logger_en(error, "Unexpected dvd raw disc write; offset=0x%016" PRIX64 ", size=0x%08" PRIX32 " -> IGNORED!",
							curr_rw_request->offset, curr_rw_request->size);
					} else {
						uint64_t offset = curr_rw_request->offset;
						if (curr_rw_request->handle == PARTITION0_HANDLE) {
							// Partition zero can access the whole disk, so figure out the target offset first
							offset = disk_offset_to_partition_offset(curr_rw_request->offset, dev);
						}
						io_result.status = fatx::driver::get(dev).write_raw_partition(offset, curr_rw_request->size, curr_rw_request->buffer.get());
						if (io_result.status == STATUS_SUCCESS) {
							io_result.info = static_cast<info_t>(curr_rw_request->size);
						}
					}
				}
				else {
					if (dev == DEV_CDROM) [[unlikely]] {
						// Write to a dvd file (this should not happen...)
						io_result.status = STATUS_IO_DEVICE_ERROR;
						logger_en(error, "Unexpected dvd file write; offset=0x%016" PRIX64 ", size=0x%08" PRIX32 " -> IGNORED!",
							curr_rw_request->offset, curr_rw_request->size);
					} else {
						if (!fs->is_open()) [[unlikely]] {
							// Write operation on a directory (this should not happen...)
							logger_en(warn, "Write operation to directory handle 0x%08" PRIX32 " with path %s", it->first, it->second->path.c_str());
							break;
						}
						fs->seekg(curr_rw_request->offset);
						fs->write(curr_rw_request->buffer.get(), curr_rw_request->size);
						fatx::DIRENT file_dirent = static_cast<file_info_fatx_t &&>(*it->second).dirent;
						if (!fs->good() || (fatx::driver::get(dev).append_clusters_to_file(file_dirent, curr_rw_request->offset, curr_rw_request->size, it->second->path) != STATUS_SUCCESS)) {
							fs->clear();
							logger_en(info, "Write operation to file handle 0x%08" PRIX32 " with path %s, offset=0x%016" PRIX64 ", size=0x%08" PRIX32 " -> FAILED!",
								it->first, it->second->path.c_str(), curr_rw_request->offset, curr_rw_request->size);
						} else {
							static_cast<file_info_fatx_t &&>(*it->second).set_dirent(file_dirent);
							static_cast<file_info_fatx_t &&>(*it->second).last_access_time(curr_rw_request->timestamp);
							static_cast<file_info_fatx_t &&>(*it->second).last_write_time(curr_rw_request->timestamp);
							io_result.status = STATUS_SUCCESS;
							io_result.info = static_cast<info_t>(curr_rw_request->size);
							logger_en(info, "Write operation to file handle 0x%08" PRIX32 ", offset=0x%016" PRIX64 ", size=0x%08" PRIX32 " -> OK!",
								it->first, curr_rw_request->offset, curr_rw_request->size);
						}
					}
				}
			}
			break;

			case request_type_t::remove: {
				if (dev == DEV_CDROM) [[unlikely]] {
					// File delete operation to the dvd disc (this should not happen...)
					io_result.status = STATUS_IO_DEVICE_ERROR;
					logger_en(error, "Unexpected dvd file delete operation -> IGNORED!");
				} else {
					file_info_fatx_t *file_info_fatx = (file_info_fatx_t *)(it->second.get());
					fatx::driver::get(dev).delete_dirent_for_file(file_info_fatx->dirent);

					// NOTE: We don't need to delete the actual host file, because file deletion is set in the fatx dirents, and not by its presence of the host
				}
			}
			break;

			default:
				logger_en(warn, "Unknown io request of type %" PRId32, host_io_request->type);
			}

			host_io_request->info.header = io_result;
			s_completed_io_mtx.lock();
			s_completed_io_info.emplace(host_io_request->id, std::move(host_io_request));
			s_completed_io_mtx.unlock();
		}
	}

	static void
	enqueue_io_packet(std::unique_ptr<request_t> host_io_request)
	{
		// If the I/O thread is currently holding the lock, we won't wait and instead retry the operation later
		if (s_queue_mtx.try_lock()) {
			s_curr_io_queue.push_back(std::move(host_io_request));
			// Signal that there's a new packet to process
			s_pending_io.test_and_set();
			s_pending_io.notify_one();
			s_queue_mtx.unlock();
		}
		else {
			s_pending_io_vec.push_back(std::move(host_io_request));
			pending_packets = true;
		}
	}

	void
	submit_io_packet(uint32_t addr)
	{
		packed_request_t io_request;
		mem_read_block_virt(s_lc86cpu, addr, sizeof(packed_request_t), (uint8_t *)&io_request);

		if (request_type_t io_type = IO_GET_TYPE(io_request.header.type); io_type == open) {
			std::unique_ptr<request_oc_t> host_io_request = std::make_unique<request_oc_t>();
			host_io_request->id = io_request.header.id;
			host_io_request->type = io_request.header.type;
			host_io_request->initial_size = io_request.m_oc.initial_size;
			host_io_request->size = io_request.m_oc.size;
			host_io_request->handle = io_request.m_oc.handle;
			host_io_request->path = new char[io_request.m_oc.size + 1];
			mem_read_block_virt(s_lc86cpu, io_request.m_oc.path, host_io_request->size, (uint8_t *)host_io_request->path);
			host_io_request->path[io_request.m_oc.size] = '\0';
			host_io_request->attributes = io_request.m_oc.attributes;
			host_io_request->timestamp = io_request.m_oc.timestamp;
			host_io_request->desired_access = io_request.m_oc.desired_access;
			host_io_request->create_options = io_request.m_oc.create_options;
			enqueue_io_packet(std::move(host_io_request));
		}
		else {
			if ((io_type == write) || (io_type == read)) {
				std::unique_ptr<request_rw_t> host_io_request = std::make_unique<request_rw_t>();
				host_io_request->id = io_request.header.id;
				host_io_request->type = io_request.header.type;
				host_io_request->offset = io_request.m_rw.offset;
				host_io_request->address = io_request.m_rw.address;
				host_io_request->size = io_request.m_rw.size;
				host_io_request->handle = io_request.m_rw.handle;
				host_io_request->timestamp = io_request.m_rw.timestamp;
				host_io_request->buffer = std::make_unique_for_overwrite<char[]>(host_io_request->size);
				if (io_type == write) {
					mem_read_block_virt(s_lc86cpu, host_io_request->address, host_io_request->size, reinterpret_cast<uint8_t *>(host_io_request->buffer.get()));
				}
				enqueue_io_packet(std::move(host_io_request));
			} else {
				std::unique_ptr<request_t> host_io_request = std::make_unique<request_t>();
				host_io_request->id = io_request.header.id;
				host_io_request->type = io_request.header.type;
				host_io_request->handle = io_request.m_xx.handle;
				enqueue_io_packet(std::move(host_io_request));
			}
		}
	}

	void
	flush_pending_packets()
	{
		if (!s_pending_io_vec.empty()) {
			// If the I/O thread is currently holding the lock, we won't wait and instead retry the operation later
			if (s_queue_mtx.try_lock()) {
				std::for_each(s_pending_io_vec.begin(), s_pending_io_vec.end(), [](auto &&packet) {
					s_curr_io_queue.push_back(std::move(packet));
					});
				s_pending_io_vec.clear();
				pending_packets = false;
				// Signal that there are new packets to process
				s_pending_io.test_and_set();
				s_pending_io.notify_one();
				s_queue_mtx.unlock();
			}
		}
	}

	void
	query_io_packet(uint32_t addr)
	{
		if (s_completed_io_mtx.try_lock()) { // don't wait if the I/O thread is currently using the map
			info_block_oc_t block;
			mem_read_block_virt(s_lc86cpu, addr, sizeof(info_block_oc_t), (uint8_t *)&block);
			auto it = s_completed_io_info.find(block.header.id);
			if (it != s_completed_io_info.end()) {
				uint64_t size_of_request;
				request_t *request = it->second.get();
				if ((IO_GET_TYPE(request->type) == read) && (request->info.header.status == STATUS_SUCCESS)) {
					// Do the transfer here instead of the IO thread to avoid races with the cpu thread
					request_rw_t *request_rw = (request_rw_t *)request;
					mem_write_block_virt(s_lc86cpu, request_rw->address, request_rw->size, request_rw->buffer.get());
				}
				if (IO_GET_TYPE(request->type) == open) {
					block = request->info;
					size_of_request = sizeof(info_block_oc_t);
				} else {
					block.header = request->info.header;
					size_of_request = sizeof(info_block_t);
				}
				block.header.ready = 1;
				mem_write_block_virt(s_lc86cpu, addr, size_of_request, &block);
				s_completed_io_info.erase(it);
			}
			s_completed_io_mtx.unlock();
		}
	}

	bool
	init(const init_info_t &init_info, cpu_t *cpu)
	{
		s_lc86cpu = cpu;
		g_nxbx_dir = init_info.m_nxbx_dir;
		std::filesystem::path hdd_dir = g_nxbx_dir;
		hdd_dir /= "Harddisk/";
		hdd_dir = to_slash_separator(hdd_dir);
		if (!::create_directory(hdd_dir)) {
			return false;
		}
		for (unsigned i = 1; i < XBOX_NUM_OF_HDD_PARTITIONS; ++i) {
			if (!::create_directory(to_slash_separator(hdd_dir / ("Partition" + std::to_string(i))))) {
				return false;
			}
		}

		if (!fatx::driver::init(hdd_dir)) {
			logger_en(error, "Failed to initialize the FATX driver");
			return false;
		}

		if (init_info.m_input_type == input_t::xiso) {
			g_xbe_name = "default.xbe";
			g_hdd_dir = hdd_dir;
			g_dvd_dir = std::filesystem::path(init_info.m_input_path).remove_filename();
			g_xbe_path_xbox = "\\Device\\CdRom0\\" + g_xbe_name;
			s_dvd_input_type = input_t::xiso;
		}
		else {
			std::filesystem::path local_xbe_path = to_slash_separator(std::filesystem::path(init_info.m_input_path));
			g_xbe_name = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(local_xbe_path.filename().string());
			g_hdd_dir = hdd_dir;
			g_dvd_dir = local_xbe_path.remove_filename();
			g_xbe_path_xbox = "\\Device\\CdRom0\\" + g_xbe_name;
			s_dvd_input_type = input_t::xbe;
			if (g_dvd_dir.string().starts_with(g_hdd_dir.string())) {
				// XBE is installed inside a HDD partition, so set the dvd drive to be empty by setting the dvd path to an invalid directory
				size_t partition_num_off = g_hdd_dir.string().size() + 9;
				std::string xbox_hdd_dir = "\\Device\\Harddisk0\\Partition" + std::to_string(g_dvd_dir.string()[partition_num_off] - '0');
				std::string xbox_remaining_hdd_dir = g_dvd_dir.string().substr(partition_num_off + 1);
				std::replace(xbox_remaining_hdd_dir.begin(), xbox_remaining_hdd_dir.end(), '/', '\\'); // convert to xbox path separator
				g_xbe_path_xbox = util::traits_cast<util::xbox_char_traits, char, std::char_traits<char>>(xbox_hdd_dir + xbox_remaining_hdd_dir + g_xbe_name.c_str());
				g_dvd_dir = "";
				console::get().update_tray_state(tray_state::no_media, false);
			}
		}

		if (init_info.m_sync_part >= 0) {
			if (init_info.m_sync_part) {
				fatx::driver::get(init_info.m_sync_part + DEV_PARTITION0).sync_partition_files();
			}
			else {
				for (unsigned partition_num = 1; partition_num < XBOX_NUM_OF_HDD_PARTITIONS; ++partition_num) {
					fatx::driver::get(partition_num + DEV_PARTITION0).sync_partition_files();
				}
			}
		}

		add_device_handles();

		s_jthr = std::jthread(&io::worker);

		return true;
	}

	void
	stop()
	{
		if (s_jthr.joinable()) {
			// Signal the I/O thread that it needs to exit
			s_jthr.request_stop();
			s_queue_mtx.lock();
			s_pending_io.test_and_set();
			s_pending_io.notify_one();
			s_queue_mtx.unlock();
			s_jthr.join();
		}
	}
}
