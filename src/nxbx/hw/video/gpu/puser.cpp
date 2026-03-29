// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.h"
#include "puser.hpp"
#include "pmc.hpp"
#include "pfifo.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include <cinttypes>

#define MODULE_NAME puser


/** Private device implementation **/
class puser::Impl
{
public:
	bool init(cpu *cpu, nv2a *gpu);
	void updateIo() { updateIo(true); }
	template<bool log = false>
	uint32_t read32(uint32_t addr);
	template<bool log = false>
	void write32(uint32_t addr, const uint32_t value);

private:
	bool updateIo(bool is_update);
	template<bool is_write>
	auto getIoFunc(bool log, bool is_be);

	// connected devices
	pmc *m_pmc;
	pfifo *m_pfifo;
	cpu_t *m_lc86cpu;
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PUSER_DMA_PUT, "NV_PUSER_DMA_PUT" },
		{ NV_PUSER_DMA_GET, "NV_PUSER_DMA_GET" },
		{ NV_PUSER_REF, "NV_PUSER_REF" }
	};
};

template<bool log>
void puser::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		nv2a_log_write();
	}

	uint32_t chan_id = ((addr - NV_PUSER_BASE) >> 16) & (NV2A_MAX_NUM_CHANNELS - 1); // addr increases of 0x10000 for each channel
	uint32_t curr_chan_info = m_pfifo->read32(NV_PFIFO_CACHE1_PUSH1);
	uint32_t curr_chan_id = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_CHID;
	uint32_t curr_chan_mode = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_MODE;

	if (curr_chan_id == chan_id) {
		if (curr_chan_mode == (NV_PFIFO_CACHE1_PUSH1_MODE)) {

			// NV_USER is a window to the corresponding pfifo registers
			switch (addr)
			{
			case NV_PUSER_DMA_PUT:
				m_pfifo->write32(NV_PFIFO_CACHE1_DMA_PUT, value);
				break;

			case NV_PUSER_DMA_GET:
				// This register is read-only
				break;

			case NV_PUSER_REF:
				// This register is read-only
				break;

			default:
				nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
			}
		}
		else {
			nxbx_fatal("PIO channel mode is not supported");
		}
	}
	else {
		// This should save the current channel state to ramfc and do a context switch
		nxbx_fatal("Context switch is not supported");
	}
}

template<bool log>
uint32_t puser::Impl::read32(uint32_t addr)
{
	uint32_t value = 0;
	uint32_t chan_id = ((addr - NV_PUSER_BASE) >> 16) & (NV2A_MAX_NUM_CHANNELS - 1); // addr increases of 0x10000 for each channel
	uint32_t curr_chan_info = m_pfifo->read32(NV_PFIFO_CACHE1_PUSH1);
	uint32_t curr_chan_id = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_CHID;
	uint32_t curr_chan_mode = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_MODE;

	if (curr_chan_id == chan_id) {
		if (curr_chan_mode == (NV_PFIFO_CACHE1_PUSH1_MODE)) {

			// NV_USER is a window to the corresponding pfifo registers
			switch (addr)
			{
			case NV_PUSER_DMA_PUT:
				value = m_pfifo->read32(NV_PFIFO_CACHE1_DMA_PUT);
				break;

			case NV_PUSER_DMA_GET:
				value = m_pfifo->read32(NV_PFIFO_CACHE1_DMA_GET);
				break;

			case NV_PUSER_REF:
				value = m_pfifo->read32(NV_PFIFO_CACHE1_REF);
				break;

			default:
				nxbx_fatal("Unhandled read at address 0x%" PRIX32, addr);
			}
		}
		else {
			nxbx_fatal("PIO channel mode is not supported");
		}
	}
	else {
		// This should save the current channel state to ramfc and do a context switch
		nxbx_fatal("Context switch is not supported");
	}

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

template<bool is_write>
auto puser::Impl::getIoFunc(bool log, bool is_be)
{
	if constexpr (is_write) {
		if (log) {
			return is_be ? nv2a_write<puser::Impl, uint32_t, &puser::Impl::write32<true>, big> : nv2a_write<puser::Impl, uint32_t, &puser::Impl::write32<true>, le>;
		}
		else {
			return is_be ? nv2a_write<puser::Impl, uint32_t, &puser::Impl::write32<false>, big> : nv2a_write<puser::Impl, uint32_t, &puser::Impl::write32<false>, le>;
		}
	}
	else {
		if (log) {
			return is_be ? nv2a_read<puser::Impl, uint32_t, &puser::Impl::read32<true>, big> : nv2a_read<puser::Impl, uint32_t, &puser::Impl::read32<true>, le>;
		}
		else {
			return is_be ? nv2a_read<puser::Impl, uint32_t, &puser::Impl::read32<false>, big> : nv2a_read<puser::Impl, uint32_t, &puser::Impl::read32<false>, le>;
		}
	}
}

bool
puser::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) &NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PUSER_BASE, NV_PUSER_SIZE, false,
		{
			.fnr32 = getIoFunc<false>(log, is_be),
			.fnw32 = getIoFunc<true>(log, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

bool
puser::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_pfifo = gpu->getPfifo();
	m_lc86cpu = cpu->get86cpu();
	if (!updateIo(false)) {
		return false;
	}

	return true;
}

/** Public interface implementation **/
bool puser::init(cpu *cpu, nv2a *gpu)
{
	return m_impl->init(cpu, gpu);
}

void puser::updateIo()
{
	m_impl->updateIo();
}

uint32_t puser::read32(uint32_t addr)
{
	return m_impl->read32<false>(addr);
}

void puser::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false>(addr, value);
}

puser::puser() : m_impl{std::make_unique<puser::Impl>()} {}
puser::~puser() {}
