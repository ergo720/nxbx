// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.h"
#include "../vga.hpp"
#include "pvga.hpp"
#include "pmc.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"

#define MODULE_NAME pvga


/** Private device implementation **/
class pvga::Impl
{
public:
	bool init(cpu *cpu, nv2a *gpu, vga *vga);
	void reset();
	void updateIo() { updateIo(true); }
	template<bool log = false>
	uint8_t ioRead8(uint32_t addr);
	template<bool log = false>
	void ioWrite8(uint32_t addr, const uint8_t value);
	template<bool log = false>
	void ioWrite16(uint32_t addr, const uint16_t value);
	template<bool log = false>
	uint8_t memRead8(uint32_t addr);
	template<bool log = false>
	uint16_t memRead16(uint32_t addr);
	template<bool log = false>
	void memWrite8(uint32_t addr, const uint8_t value);
	template<bool log = false>
	void memWrite16(uint32_t addr, const uint16_t value);

private:
	void prmvgaLogRead(uint32_t addr, uint32_t value);
	void prmvgaLogWrite(uint32_t addr, uint32_t value);
	bool updateIo(bool is_update);
	
	// connected devices
	pmc *m_pmc;
	vga *m_vga;
	cpu_t *m_lc86cpu;
};

template<bool log>
uint8_t pvga::Impl::ioRead8(uint32_t addr)
{
	uint8_t value = m_vga->ioRead8(addr);

	if constexpr (log) {
		prmvgaLogRead(addr, value);
	}

	return value;
}

template<bool log>
void pvga::Impl::ioWrite8(uint32_t addr, const uint8_t value)
{
	if constexpr (log) {
		prmvgaLogWrite(addr, value);
	}

	m_vga->ioWrite8(addr, value);
}

template<bool log>
void pvga::Impl::ioWrite16(uint32_t addr, const uint16_t value)
{
	if constexpr (log) {
		prmvgaLogWrite(addr, value);
	}

	m_vga->ioWrite16(addr, value);
}

template<bool log>
uint8_t pvga::Impl::memRead8(uint32_t addr)
{
	uint8_t value = m_vga->memRead8(addr);

	if constexpr (log) {
		prmvgaLogRead(addr, value);
	}

	return value;
}

template<bool log>
uint16_t pvga::Impl::memRead16(uint32_t addr)
{
	uint16_t value = m_vga->memRead16(addr);

	if constexpr (log) {
		prmvgaLogRead(addr, value);
	}

	return value;
}

template<bool log>
void pvga::Impl::memWrite8(uint32_t addr, const uint8_t value)
{
	if constexpr (log) {
		prmvgaLogWrite(addr, value);
	}

	m_vga->memWrite8(addr, value);
}

template<bool log>
void pvga::Impl::memWrite16(uint32_t addr, const uint16_t value)
{
	if constexpr (log) {
		prmvgaLogWrite(addr, value);
	}

	m_vga->memWrite16(addr, value);
}

void
pvga::Impl::prmvgaLogRead(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pvga, false>("Read at 0x%08X of value 0x%08X", addr, value);
}

void
pvga::Impl::prmvgaLogWrite(uint32_t addr, uint32_t value)
{
	logger<log_lv::debug, log_module::pvga, false>("Write at 0x%08X of value 0x%08X", addr, value);
}

bool
pvga::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	// PRMVIO is an alias for the vga sequencer and graphics controller ports
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PRMVIO_BASE, NV_PRMVIO_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga::Impl, uint8_t, &pvga::Impl::ioRead8<true>, NV_PRMVIO_BASE> : cpu_read<pvga::Impl, uint8_t, &pvga::Impl::ioRead8<false>, NV_PRMVIO_BASE>,
			.fnw8 = log ? cpu_write<pvga::Impl, uint8_t, &pvga::Impl::ioWrite8<true>, NV_PRMVIO_BASE> : cpu_write<pvga::Impl, uint8_t, &pvga::Impl::ioWrite8<false>, NV_PRMVIO_BASE>,
			.fnw16 = log ? cpu_write<pvga::Impl, uint16_t, &pvga::Impl::ioWrite16<true>, NV_PRMVIO_BASE> : cpu_write<pvga::Impl, uint16_t, &pvga::Impl::ioWrite16<false>, NV_PRMVIO_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	// PRMCIO is an alias for the vga attribute controller and crt controller ports
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PRMCIO_BASE, NV_PRMCIO_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga::Impl, uint8_t, &pvga::Impl::ioRead8<true>, NV_PRMCIO_BASE> : cpu_read<pvga::Impl, uint8_t, &pvga::Impl::ioRead8<false>, NV_PRMCIO_BASE>,
			.fnw8 = log ? cpu_write<pvga::Impl, uint8_t, &pvga::Impl::ioWrite8<true>, NV_PRMCIO_BASE> : cpu_write<pvga::Impl, uint8_t, &pvga::Impl::ioWrite8<false>, NV_PRMCIO_BASE>,
			.fnw16 = log ? cpu_write<pvga::Impl, uint16_t, &pvga::Impl::ioWrite16<true>, NV_PRMCIO_BASE> : cpu_write<pvga::Impl, uint16_t, &pvga::Impl::ioWrite16<false>, NV_PRMCIO_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	// PRMDIO is an alias for the vga digital-to-analog converter (DAC) ports
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PRMDIO_BASE, NV_PRMDIO_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga::Impl, uint8_t, &pvga::Impl::ioRead8<true>, NV_PRMDIO_BASE> : cpu_read<pvga::Impl, uint8_t, &pvga::Impl::ioRead8<false>, NV_PRMDIO_BASE>,
			.fnw8 = log ? cpu_write<pvga::Impl, uint8_t, &pvga::Impl::ioWrite8<true>, NV_PRMDIO_BASE> : cpu_write<pvga::Impl, uint8_t, &pvga::Impl::ioWrite8<false>, NV_PRMDIO_BASE>,
			.fnw16 = log ? cpu_write<pvga::Impl, uint16_t, &pvga::Impl::ioWrite16<true>, NV_PRMDIO_BASE> : cpu_write<pvga::Impl, uint16_t, &pvga::Impl::ioWrite16<false>, NV_PRMDIO_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	// PRMVGA is an alias for the vga memory window
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PRMVGA_BASE, NV_PRMVGA_SIZE, false,
		{
			.fnr8 = log ? cpu_read<pvga::Impl, uint8_t, &pvga::Impl::memRead8<true>, NV_PRMVGA_BASE> : cpu_read<pvga::Impl, uint8_t, &pvga::Impl::memRead8<false>, NV_PRMVGA_BASE>,
			.fnr16 = log ? cpu_read<pvga::Impl, uint16_t, &pvga::Impl::memRead16<true>, NV_PRMVGA_BASE> : cpu_read<pvga::Impl, uint16_t, &pvga::Impl::memRead16<false>, NV_PRMVGA_BASE>,
			.fnw8 = log ? cpu_write<pvga::Impl, uint8_t, &pvga::Impl::memWrite8<true>, NV_PRMVGA_BASE> : cpu_write<pvga::Impl, uint8_t, &pvga::Impl::memWrite8<false>, NV_PRMVGA_BASE>,
			.fnw16 = log ? cpu_write<pvga::Impl, uint16_t, &pvga::Impl::memWrite16<true>, NV_PRMVGA_BASE> : cpu_write<pvga::Impl, uint16_t, &pvga::Impl::memWrite16<false>, NV_PRMVGA_BASE>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pvga::Impl::reset()
{
	m_vga->reset();
}

bool
pvga::Impl::init(cpu *cpu, nv2a *gpu, vga *vga)
{
	m_pmc = gpu->getPmc();
	m_lc86cpu = cpu->get86cpu();
	m_vga = vga;
	if (!updateIo(false)) {
		return false;
	}

	// Don't reset here, because vga will be reset when it's initialized later
	return true;
}

/** Public interface implementation **/
bool pvga::init(cpu *cpu, nv2a *gpu, vga *vga)
{
	return m_impl->init(cpu, gpu, vga);
}

void pvga::reset()
{
	m_impl->reset();
}

void pvga::updateIo()
{
	m_impl->updateIo();
}

pvga::pvga() : m_impl{std::make_unique<pvga::Impl>()} {}
pvga::~pvga() {}
