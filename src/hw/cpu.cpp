// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "cpu.hpp"
#include "pic.hpp"
#include "pit.hpp"
#include "cmos.hpp"
#include "../logger.hpp"
#include "../kernel.hpp"
#include "../pe.hpp"
#include "../clock.hpp"
#include "../init.hpp"
#include <fstream>
#include <cinttypes>
#include <array>


static consteval bool
check_cpu_log_lv()
{
	return ((std::underlying_type_t<log_level>)(log_level::debug) == (std::underlying_type_t<log_lv>)(log_lv::debug)) &&
		((std::underlying_type_t<log_level>)(log_level::info) == (std::underlying_type_t<log_lv>)(log_lv::info)) &&
		((std::underlying_type_t<log_level>)(log_level::warn) == (std::underlying_type_t<log_lv>)(log_lv::warn)) &&
		((std::underlying_type_t<log_level>)(log_level::error) == (std::underlying_type_t<log_lv>)(log_lv::error));
}

// Make sure that our log levels are the same used in lib86cpu too
static_assert(check_cpu_log_lv());


static void
cpu_logger(log_level lv, const unsigned count, const char *msg, ...)
{
	std::va_list args;
	va_start(args, msg);
	logger(static_cast<log_lv>(lv), msg, args);
	va_end(args);
}

static void
cpu_reset()
{
	// TODO: lib86cpu doesn't support resetting the cpu yet
}

void
cpu_init(const std::string &kernel, disas_syntax syntax, uint32_t use_dbg)
{
	// XXX: xbox memory hard coded to 64 MiB for now
	uint32_t ramsize = 64 * 1024 * 1024;

	// Load the nboxkrnl exe file
	std::ifstream ifs(kernel.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!ifs.is_open()) {
		throw nxbx_exp_abort("Could not open kernel file");
	}
	ifs.seekg(0, ifs.end);
	std::streampos length = ifs.tellg();
	ifs.seekg(0, ifs.beg);

	// Sanity checks on the kernel exe size
	if (length == 0) {
		throw nxbx_exp_abort("Size of kernel file detected as zero");
	}
	else if (length > ramsize) {
		throw nxbx_exp_abort("Kernel file doesn't fit inside RAM");
	}

	std::unique_ptr<char[]> krnl_buff{ new char[static_cast<unsigned>(length)] };
	if (!krnl_buff) {
		throw nxbx_exp_abort("Could not allocate kernel buffer");
	}
	ifs.read(krnl_buff.get(), length);
	ifs.close();

	// Sanity checks on the kernel exe file
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(krnl_buff.get());
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		throw nxbx_exp_abort("Kernel image has an invalid dos header signature");
	}

	PIMAGE_NT_HEADERS32 peHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(reinterpret_cast<uint8_t *>(dosHeader) + dosHeader->e_lfanew);
	if (peHeader->Signature != IMAGE_NT_SIGNATURE ||
		peHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
		peHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
		peHeader->OptionalHeader.Subsystem != IMAGE_SUBSYSTEM_NATIVE) {
		throw nxbx_exp_abort("Kernel image has an invalid nt header signature");
	}

	if (peHeader->OptionalHeader.ImageBase != KERNEL_BASE) {
		throw nxbx_exp_abort("Kernel image has an incorrect image base address");
	}

	// Init lib86cpu
	if (!LC86_SUCCESS(cpu_new(ramsize, g_cpu, pic_get_interrupt, "nboxkrnl"))) {
		throw nxbx_exp_abort("Failed to create cpu instance");
	}

	register_log_func(cpu_logger);

	cpu_set_flags(g_cpu, static_cast<uint32_t>(syntax) | (use_dbg ? CPU_DBG_PRESENT : 0));

	if (!LC86_SUCCESS(mem_init_region_ram(g_cpu, 0, ramsize))) {
		throw nxbx_exp_abort("Failed to initialize ram memory");
	}

	if (!LC86_SUCCESS(mem_init_region_alias(g_cpu, CONTIGUOUS_MEMORY_BASE, 0, ramsize))) {
		throw nxbx_exp_abort("Failed to initialize contiguous memory");
	}

	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, KERNEL_IO_BASE, KERNEL_IO_SIZE, true, io_handlers_t{ .fnr32 = nboxkrnl_read_handler, .fnw32 = nboxkrnl_write_handler }, g_cpu))) {
		throw nxbx_exp_abort("Failed to initialize kernel communication I/O ports");
	}

	add_reset_func(cpu_reset);

	// Load kernel exe into ram
	uint8_t *ram = get_ram_ptr(g_cpu);
	std::memcpy(&ram[peHeader->OptionalHeader.ImageBase - CONTIGUOUS_MEMORY_BASE], dosHeader, peHeader->OptionalHeader.SizeOfHeaders);

	PIMAGE_SECTION_HEADER sections = reinterpret_cast<PIMAGE_SECTION_HEADER>(reinterpret_cast<uint8_t *>(peHeader) + sizeof(IMAGE_NT_HEADERS32));
	for (uint16_t i = 0; i < peHeader->FileHeader.NumberOfSections; ++i) {
		uint8_t *dest = &ram[peHeader->OptionalHeader.ImageBase - CONTIGUOUS_MEMORY_BASE + sections[i].VirtualAddress];
		std::memcpy(dest, reinterpret_cast<uint8_t *>(dosHeader) + sections[i].PointerToRawData, sections[i].SizeOfRawData);
		if (sections[i].SizeOfRawData < sections[i].Misc.VirtualSize) {
			std::memset(dest + sections[i].SizeOfRawData, 0, sections[i].Misc.VirtualSize - sections[i].SizeOfRawData);
		}
	}

	mem_fill_block_virt(g_cpu, 0xF000, 0x1000, 0);
	uint32_t pde = 0xE3; // large, dirty, accessed, r/w, present
	mem_write_block_virt(g_cpu, 0xFF000, 4, &pde);
	for (int i = 0; i < 16; ++i) {
		mem_write_block_virt(g_cpu, 0xF000 + (i * 4), 4, &pde); // this identity maps all physical memory
		pde += 0x400000;
	}
	pde = 0x800000E3;
	for (int i = 0; i < 16; ++i) {
		mem_write_block_virt(g_cpu, 0xF800 + (i * 4), 4, &pde); // this identity maps all contiguous memory
		pde += 0x400000;
	}
	pde = 0x0000F063; // dirty, accessed, r/w, present
	mem_write_block_virt(g_cpu, 0xFC00, 4, &pde); // this maps the pts at 0xC0000000

	regs_t *regs = get_regs_ptr(g_cpu);
	regs->cs_hidden.base = 0;
	regs->es_hidden.base = 0;
	regs->ds_hidden.base = 0;
	regs->ss_hidden.base = 0;
	regs->fs_hidden.base = 0;
	regs->gs_hidden.base = 0;

	regs->cs_hidden.flags = 0xCF9F00;
	regs->es_hidden.flags = 0xCF9700;
	regs->ds_hidden.flags = 0xCF9700;
	regs->ss_hidden.flags = 0xCF9700;
	regs->fs_hidden.flags = 0xCF9700;
	regs->gs_hidden.flags = 0xCF9700;

	regs->cr0 = 0x80000001; // protected, paging
	regs->cr3 = 0xF000; // pd addr
	regs->cr4 = 0x10; // pse

	regs->esp = 0x80400000;
	regs->ebp = 0x80400000;
	regs->eip = peHeader->OptionalHeader.ImageBase + peHeader->OptionalHeader.AddressOfEntryPoint;

	// Pass eeprom and certificate keys on the stack (we use dummy all-zero keys)
	mem_fill_block_virt(g_cpu, 0x80400000, 16 * 2, 0);
}

uint64_t
cpu_check_periodic_events(uint64_t now)
{
	std::array<uint64_t, 2> dev_timeout;
	dev_timeout[0] = pit_get_next_irq_time(now);
	dev_timeout[1] = cmos_get_next_update_time(now);

	return *std::min_element(dev_timeout.begin(), dev_timeout.end());
}

static uint64_t
cpu_check_periodic_events()
{
	return cpu_check_periodic_events(get_now());
}

void
cpu_start()
{
	cpu_sync_state(g_cpu);

	lc86_status code;
	while (true) {
		code = cpu_run_until(g_cpu, cpu_check_periodic_events());
		if (code != lc86_status::timeout) [[unlikely]] {
			break;
		}
	}

	logger(log_lv::highest, "Emulation terminated with status %" PRId32 ". The error was \"%s\"", static_cast<int32_t>(code), get_last_error().c_str());
}

void
cpu_cleanup()
{
	if (g_cpu) {
		cpu_free(g_cpu);
		g_cpu = nullptr;
	}
}
