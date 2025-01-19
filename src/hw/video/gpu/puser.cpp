// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME puser


template<bool log>
void puser::write32(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	uint32_t chan_id = ((addr - NV_PUSER_BASE) >> 16) & (NV2A_MAX_NUM_CHANNELS - 1); // addr increases of 0x10000 for each channel
	uint32_t curr_chan_info = m_machine->get<pfifo>().regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_PUSH1)];
	uint32_t curr_chan_id = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_CHID_MASK;
	uint32_t curr_chan_mode = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_MODE_MASK;

	if (curr_chan_id == chan_id) {
		if (curr_chan_mode == (NV_PFIFO_CACHE1_PUSH1_MODE_MASK)) {

			// NV_USER is a window to the corresponding pfifo registers
			switch (addr)
			{
			case NV_PUSER_DMA_PUT:
				// The pb put pointer changed, so notify the pusher
				m_machine->get<pfifo>().regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_PUT)] = data;
				m_machine->get<pfifo>().signal++;
				m_machine->get<pfifo>().signal.notify_one();
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
uint32_t puser::read32(uint32_t addr)
{
	uint32_t value = 0;
	uint32_t chan_id = ((addr - NV_PUSER_BASE) >> 16) & (NV2A_MAX_NUM_CHANNELS - 1); // addr increases of 0x10000 for each channel
	uint32_t curr_chan_info = m_machine->get<pfifo>().regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_PUSH1)];
	uint32_t curr_chan_id = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_CHID_MASK;
	uint32_t curr_chan_mode = curr_chan_info & NV_PFIFO_CACHE1_PUSH1_MODE_MASK;

	if (curr_chan_id == chan_id) {
		if (curr_chan_mode == (NV_PFIFO_CACHE1_PUSH1_MODE_MASK)) {

			// NV_USER is a window to the corresponding pfifo registers
			switch (addr)
			{
			case NV_PUSER_DMA_PUT:
				value = m_machine->get<pfifo>().regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_PUT)];
				break;

			case NV_PUSER_DMA_GET:
				value = m_machine->get<pfifo>().regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_GET)];
				break;

			case NV_PUSER_REF:
				value = m_machine->get<pfifo>().regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_REF)];
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
		log_io_read();
	}

	return value;
}

template<bool is_write>
auto puser::get_io_func(bool log, bool is_be)
{
	if constexpr (is_write) {
		if (log) {
			return is_be ? nv2a_write<puser, uint32_t, &puser::write32<true>, true> : nv2a_write<puser, uint32_t, &puser::write32<true>>;
		}
		else {
			return is_be ? nv2a_write<puser, uint32_t, &puser::write32<false>, true> : nv2a_write<puser, uint32_t, &puser::write32<false>>;
		}
	}
	else {
		if (log) {
			return is_be ? nv2a_read<puser, uint32_t, &puser::read32<true>, true> : nv2a_read<puser, uint32_t, &puser::read32<true>>;
		}
		else {
			return is_be ? nv2a_read<puser, uint32_t, &puser::read32<false>, true> : nv2a_read<puser, uint32_t, &puser::read32<false>>;
		}
	}
}

bool
puser::update_io(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PUSER_BASE, NV_PUSER_SIZE, false,
		{
			.fnr32 = get_io_func<false>(log, is_be),
			.fnw32 = get_io_func<true>(log, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

bool
puser::init()
{
	if (!update_io(false)) {
		return false;
	}

	return true;
}
