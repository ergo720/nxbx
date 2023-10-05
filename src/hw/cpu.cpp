// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "cpu.hpp"
#include "pic.hpp"
#include "../kernel.hpp"
#include "../pe.hpp"
#include <fstream>
#include <cinttypes>
#include <cstdarg>


static void
cpu_logger(log_level lv, const unsigned count, const char *msg, ...)
{
	static const std::unordered_map<log_level, std::string> lv_to_str = {
		{log_level::debug, "DBG:   "},
		{log_level::info,  "INFO:  "},
		{log_level::warn,  "WARN:  "},
		{log_level::error, "ERROR: "},
	};

	std::string str;
	auto it = lv_to_str.find(lv);
	if (it == lv_to_str.end()) {
		str = std::string("UNK: ") + msg + '\n';
	}
	else {
		str = it->second + msg + '\n';
	}

	if (count > 0) {
		std::va_list args;
		va_start(args, msg);
		std::vprintf(str.c_str(), args);
		va_end(args);
	}
	else {
		std::printf("%s\n", str.c_str());
	}
}

bool
cpu_init(const std::string &executable, disas_syntax syntax, uint32_t use_dbg)
{
	// XXX: xbox memory hard coded to 64 MiB for now
	uint32_t ramsize = 64 * 1024 * 1024;

	// Load the nboxkrnl exe file
	std::ifstream ifs(executable.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!ifs.is_open()) {
		std::printf("Could not open binary file \"%s\"!\n", executable.c_str());
		return false;
	}
	ifs.seekg(0, ifs.end);
	std::streampos length = ifs.tellg();
	ifs.seekg(0, ifs.beg);

	// Sanity checks on the kernel exe size
	if (length == 0) {
		std::printf("Size of binary file \"%s\" detected as zero!\n", executable.c_str());
		return false;
	}
	else if (length > ramsize) {
		std::printf("Binary file \"%s\" doesn't fit inside RAM!\n", executable.c_str());
		return false;
	}

	std::unique_ptr<char[]> krnl_buff{ new char[static_cast<unsigned>(length)] };
	if (!krnl_buff) {
		std::printf("Could not allocate kernel buffer!\n");
		return false;
	}
	ifs.read(krnl_buff.get(), length);
	ifs.close();

	// Sanity checks on the kernel exe file
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(krnl_buff.get());
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		std::printf("Kernel image has an invalid dos header signature!\n");
		return false;
	}

	PIMAGE_NT_HEADERS32 peHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(reinterpret_cast<uint8_t *>(dosHeader) + dosHeader->e_lfanew);
	if (peHeader->Signature != IMAGE_NT_SIGNATURE ||
		peHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
		peHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
		peHeader->OptionalHeader.Subsystem != IMAGE_SUBSYSTEM_NATIVE) {
		std::printf("Kernel image has an invalid nt header signature!\n");
		return false;
	}

	if (peHeader->OptionalHeader.ImageBase != KERNEL_BASE) {
		std::printf("Kernel image has an incorrect image base address!\n");
		return false;
	}

	// Init lib86cpu
	if (!LC86_SUCCESS(cpu_new(ramsize, g_cpu, nullptr, "nboxkrnl"))) {
		std::printf("Failed to create cpu instance!\n");
		return false;
	}

	register_log_func(cpu_logger);

	cpu_set_flags(g_cpu, static_cast<uint32_t>(syntax) | (use_dbg ? CPU_DBG_PRESENT : 0) | CPU_ABORT_ON_HLT);

	if (!LC86_SUCCESS(mem_init_region_ram(g_cpu, 0, ramsize))) {
		std::printf("Failed to initialize ram memory!\n");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_alias(g_cpu, CONTIGUOUS_MEMORY_BASE, 0, ramsize))) {
		std::printf("Failed to initialize contiguous memory!\n");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, KERNEL_IO_BASE, KERNEL_IO_SIZE, true, io_handlers_t{ .fnr32 = nboxkrnl_read_handler, .fnw32 = nboxkrnl_write_handler }, g_cpu))) {
		std::printf("Failed to initialize host communication I/O ports!\n");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, 0x20, 2, true, io_handlers_t{ .fnr8 = pic_read_handler, .fnw8 = pic_write_handler }, &g_pic[0]))) {
		std::printf("Failed to initialize master pic I/O ports!\n");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, 0xA0, 2, true, io_handlers_t{ .fnr8 = pic_read_handler, .fnw8 = pic_write_handler }, &g_pic[1]))) {
		std::printf("Failed to initialize slave pic I/O ports!\n");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_io(g_cpu, 0x4D0, 2, true, io_handlers_t{ .fnr8 = pic_elcr_read_handler, .fnw8 = pic_elcr_write_handler }, g_pic))) {
		std::printf("Failed to initialize elcr I/O ports!\n");
		return false;
	}

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

	return true;
}

void
cpu_start()
{
	lc86_status	code = cpu_run(g_cpu);

	std::printf("Emulation terminated with status %" PRId32 ". The error was \"%s\"\n", static_cast<int32_t>(code), get_last_error().c_str());
	cpu_free(g_cpu);
}

void
cpu_cleanup()
{
	if (g_cpu) {
		cpu_free(g_cpu);
	}
}
