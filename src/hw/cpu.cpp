// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "machine.hpp"
#include "../logger.hpp"
#include "../kernel.hpp"
#include "../pe.hpp"
#include "../clock.hpp"
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

void
cpu::reset()
{
	// TODO: lib86cpu doesn't support resetting the cpu yet
}

bool
cpu::init(const std::string &kernel, disas_syntax syntax, uint32_t use_dbg)
{
	// FIXME: xbox memory hard coded to 64 MiB for now
	uint32_t ramsize = 64 * 1024 * 1024;

	// Load the nboxkrnl exe file
	std::ifstream ifs(kernel.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!ifs.is_open()) {
		logger(log_lv::error, "Could not open kernel file");
		return false;
	}
	ifs.seekg(0, ifs.end);
	std::streampos length = ifs.tellg();
	ifs.seekg(0, ifs.beg);

	// Sanity checks on the kernel exe size
	if (length == 0) {
		logger(log_lv::error, "Size of kernel file detected as zero");
		return false;
	}
	else if (length > ramsize) {
		logger(log_lv::error, "Kernel file doesn't fit inside ram");
		return false;
	}

	std::unique_ptr<char[]> krnl_buff{ new char[static_cast<unsigned>(length)] };
	if (!krnl_buff) {
		logger(log_lv::error, "Could not allocate kernel buffer");
		return false;
	}
	ifs.read(krnl_buff.get(), length);
	ifs.close();

	// Sanity checks on the kernel exe file
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(krnl_buff.get());
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		logger(log_lv::error, "Kernel image has an invalid dos header signature");
		return false;
	}

	PIMAGE_NT_HEADERS32 peHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(reinterpret_cast<uint8_t *>(dosHeader) + dosHeader->e_lfanew);
	if (peHeader->Signature != IMAGE_NT_SIGNATURE ||
		peHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
		peHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
		peHeader->OptionalHeader.Subsystem != IMAGE_SUBSYSTEM_NATIVE) {
		logger(log_lv::error, "Kernel image has an invalid nt header signature");
		return false;
	}

	if (peHeader->OptionalHeader.ImageBase != KERNEL_BASE) {
		logger(log_lv::error, "Kernel image has an incorrect image base address");
		return false;
	}

	// Init lib86cpu
	if (!LC86_SUCCESS(cpu_new(ramsize, m_lc86cpu, { get_interrupt_for_cpu, &m_machine->get<pic>() }, "nboxkrnl"))) {
		logger(log_lv::error, "Failed to create cpu instance");
		return false;
	}

	register_log_func(cpu_logger);

	cpu_set_flags(m_lc86cpu, static_cast<uint32_t>(syntax) | (use_dbg ? CPU_DBG_PRESENT : 0));

	if (!LC86_SUCCESS(mem_init_region_ram(m_lc86cpu, 0, ramsize))) {
		logger(log_lv::error, "Failed to initialize ram memory");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_alias(m_lc86cpu, CONTIGUOUS_MEMORY_BASE, 0, ramsize))) {
		logger(log_lv::error, "Failed to initialize contiguous memory");
		return false;
	}

	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, kernel::IO_BASE, kernel::IO_SIZE, true, { .fnr32 = kernel::read_handler, .fnw32 = kernel::write_handler }, m_lc86cpu))) {
		logger(log_lv::error, "Failed to initialize kernel communication io ports");
		return false;
	}

	// Load kernel exe into ram
	uint8_t *ram = get_ram_ptr(m_lc86cpu);
	std::memcpy(&ram[peHeader->OptionalHeader.ImageBase - CONTIGUOUS_MEMORY_BASE], dosHeader, peHeader->OptionalHeader.SizeOfHeaders);

	PIMAGE_SECTION_HEADER sections = reinterpret_cast<PIMAGE_SECTION_HEADER>(reinterpret_cast<uint8_t *>(peHeader) + sizeof(IMAGE_NT_HEADERS32));
	for (uint16_t i = 0; i < peHeader->FileHeader.NumberOfSections; ++i) {
		uint8_t *dest = &ram[peHeader->OptionalHeader.ImageBase - CONTIGUOUS_MEMORY_BASE + sections[i].VirtualAddress];
		std::memcpy(dest, reinterpret_cast<uint8_t *>(dosHeader) + sections[i].PointerToRawData, sections[i].SizeOfRawData);
		if (sections[i].SizeOfRawData < sections[i].Misc.VirtualSize) {
			std::memset(dest + sections[i].SizeOfRawData, 0, sections[i].Misc.VirtualSize - sections[i].SizeOfRawData);
		}
	}

	mem_fill_block_virt(m_lc86cpu, 0xF000, 0x1000, 0);
	uint32_t pde = 0xE3; // large, dirty, accessed, r/w, present
	mem_write_block_virt(m_lc86cpu, 0xFF000, 4, &pde);
	for (int i = 0; i < 16; ++i) {
		mem_write_block_virt(m_lc86cpu, 0xF000 + (i * 4), 4, &pde); // this identity maps all physical memory
		pde += 0x400000;
	}
	pde = 0x800000E3;
	for (int i = 0; i < 16; ++i) {
		mem_write_block_virt(m_lc86cpu, 0xF800 + (i * 4), 4, &pde); // this identity maps all contiguous memory
		pde += 0x400000;
	}
	pde = 0x0000F063; // dirty, accessed, r/w, present
	mem_write_block_virt(m_lc86cpu, 0xFC00, 4, &pde); // this maps the pts at 0xC0000000

	regs_t *regs = get_regs_ptr(m_lc86cpu);
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
	mem_fill_block_virt(m_lc86cpu, 0x80400000, 16 * 2, 0);

	return true;
}

uint64_t
cpu::check_periodic_events(uint64_t now)
{
	std::array<uint64_t, 3> dev_timeout;
	dev_timeout[0] = m_machine->get<pit>().get_next_irq_time(now);
	dev_timeout[1] = m_machine->get<cmos>().get_next_update_time(now);
	dev_timeout[2] = m_machine->get<nv2a>().get_next_update_time(now);

	return *std::min_element(dev_timeout.begin(), dev_timeout.end());
}

uint64_t
cpu::check_periodic_events()
{
	return check_periodic_events(timer::get_now());
}

void
cpu::start()
{
	cpu_sync_state(m_lc86cpu);

	lc86_status code;
	while (true) {
		code = cpu_run_until(m_lc86cpu, check_periodic_events());
		if (code != lc86_status::timeout) [[unlikely]] {
			break;
		}
	}

	logger(log_lv::highest, "Emulation terminated with status %" PRId32 ". The error was \"%s\"", static_cast<int32_t>(code), get_last_error().c_str());
}

void
cpu::exit()
{
	cpu_exit(m_lc86cpu);
}

void
cpu::deinit()
{
	if (m_lc86cpu) {
		cpu_free(m_lc86cpu);
		m_lc86cpu = nullptr;
	}
}
