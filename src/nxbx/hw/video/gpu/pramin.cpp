// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "pramin.hpp"
#include "pmc.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"

#define MODULE_NAME pramin

#define RAMIN_UNIT_SIZE 64


/** Private device implementation **/
class pramin::Impl
{
public:
	bool init(cpu *cpu, nv2a *gpu);
	void updateIo() { updateIo(true); }
	uint32_t read32(uint32_t offset);
	void write32(uint32_t offset, const uint32_t value);

private:
	template<typename T, bool log = false>
	T read(uint32_t addr);
	template<typename T, bool log = false>
	void write(uint32_t addr, const T value);
	void logRead(uint32_t addr, uint32_t value);
	void logWrite(uint32_t addr, uint32_t value);
	bool updateIo(bool is_update);
	template<bool is_write, typename T>
	auto getIoFunc(bool log, bool is_be);
	uint32_t ramin_to_ram_addr(uint32_t ramin_addr);

	uint8_t *m_ram;
	uint32_t m_ramsize;
	// connected devices
	pmc *m_pmc;
	cpu_t *m_lc86cpu;
};

template<typename T, bool log>
T pramin::Impl::read(uint32_t addr)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr - NV_PRAMIN_BASE);
	T value = *(T *)ram_ptr;

	if constexpr (log) {
		logRead(addr, value);
	}

	return value;
}

template<typename T, bool log>
void pramin::Impl::write(uint32_t addr, const T value)
{
	if constexpr (log) {
		logWrite(addr, value);
	}

	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(addr - NV_PRAMIN_BASE);
	*(T *)ram_ptr = value;
}

uint32_t pramin::Impl::read32(uint32_t offset)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(offset);
	return *(uint32_t *)ram_ptr;
}

void pramin::Impl::write32(uint32_t offset, const uint32_t value)
{
	uint8_t *ram_ptr = m_ram + ramin_to_ram_addr(offset);
	*(uint32_t *)ram_ptr = value;
}

uint32_t
pramin::Impl::ramin_to_ram_addr(uint32_t ramin_addr)
{
	return m_ramsize - (ramin_addr - (ramin_addr % RAMIN_UNIT_SIZE)) - RAMIN_UNIT_SIZE + (ramin_addr % RAMIN_UNIT_SIZE);
}

void
pramin::Impl::logRead(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pramin, false>("Read at NV_PRAMIN_BASE + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PRAMIN_BASE, addr, value);
}

void
pramin::Impl::logWrite(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pramin, false>("Write at NV_PRAMIN_BASE + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PRAMIN_BASE, addr, value);
}

template<bool is_write, typename T>
auto pramin::Impl::getIoFunc(bool log, bool is_be)
{
	if constexpr (is_write) {
		if (log) {
			return is_be ? nv2a_write<pramin::Impl, T, &pramin::Impl::write<T, true>, big> : nv2a_write<pramin::Impl, T, &pramin::Impl::write<T, true>, le>;
		}
		else {
			return is_be ? nv2a_write<pramin::Impl, T, &pramin::Impl::write<T, false>, big> : nv2a_write<pramin::Impl, T, &pramin::Impl::write<T, false>, le>;
		}
	}
	else {
		if (log) {
			return is_be ? nv2a_read<pramin::Impl, T, &pramin::Impl::read<T, true>, big> : nv2a_read<pramin::Impl, T, &pramin::Impl::read<T, true>, le>;
		}
		else {
			return is_be ? nv2a_read<pramin::Impl, T, &pramin::Impl::read<T, false>, big> : nv2a_read<pramin::Impl, T, &pramin::Impl::read<T, false>, le>;
		}
	}
}

bool
pramin::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PRAMIN_BASE, NV_PRAMIN_SIZE, false,
		{
			.fnr8 = getIoFunc<false, uint8_t>(log, is_be),
			.fnr16 = getIoFunc<false, uint16_t>(log, is_be),
			.fnr32 = getIoFunc<false, uint32_t>(log, is_be),
			.fnw8 = getIoFunc<true, uint8_t>(log, is_be),
			.fnw16 = getIoFunc<true, uint16_t>(log, is_be),
			.fnw32 = getIoFunc<true, uint32_t>(log, is_be),
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

bool
pramin::Impl::init(cpu *cpu, nv2a *gpu)
{
	// Tested and confirmed with a Retail 1.0 xbox. The ramin starts from the end of vram, and it's the last MiB of it. It's also addressed in reverse order,
	// with block units of 64 bytes each.
	/*
	ramin -> vram
	0 -> 0xF3FFFFC0
	32 -> 0xF3FFFFE0
	64 -> 0xF3FFFF80
	96 -> 0xF3FFFFA0
	
	- 32 bytes, -- 64 bytes block units
	----------ramin
	abcd  efgh
	ghef  cdab
	----------vram
	*/

	m_pmc = gpu->getPmc();
	m_lc86cpu = cpu->get86cpu();
	// Store ram size in this object. This way, we don't need to query NV_PFB_CSTATUS in ramin_to_ram_addr (which is accessed from the fifo thread from getDmaObj)
	m_ram = get_ram_ptr(m_lc86cpu);
	m_ramsize = cpu->getRamsize();

	if (!updateIo(false)) {
		return false;
	}

	return true;
}

/** Public interface implementation **/
bool pramin::init(cpu *cpu, nv2a *gpu)
{
	return m_impl->init(cpu, gpu);
}

void pramin::updateIo()
{
	m_impl->updateIo();
}

uint32_t pramin::read32(uint32_t offset)
{
	return m_impl->read32(offset);
}

void pramin::write32(uint32_t offset, const uint32_t value)
{
	m_impl->write32(offset, value);
}

pramin::pramin() : m_impl{std::make_unique<pramin::Impl>()} {}
pramin::~pramin() {}
