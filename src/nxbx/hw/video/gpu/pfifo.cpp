// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"
#include <functional>

#define MODULE_NAME pfifo


template<bool log, engine_enabled enabled>
void pfifo::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		log_write(addr, value);
	}

	switch (addr)
	{
	case NV_PFIFO_INTR_0:
		REG_PFIFO(addr) &= ~value;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PFIFO_INTR_EN_0:
		REG_PFIFO(addr) = value;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PFIFO_CACHE1_DMA_PUSH:
		// Mask out read-only bits
		REG_PFIFO(addr) = (value & ~(NV_PFIFO_CACHE1_DMA_PUSH_STATE | NV_PFIFO_CACHE1_DMA_PUSH_BUFFER));
		break;

	case NV_PFIFO_CACHE1_DMA_PUT:
		REG_PFIFO(addr) = value;
		pusher();
		break;

	case NV_PFIFO_CACHE1_DMA_GET:
		REG_PFIFO(addr) = value;
		pusher();
		break;

	case NV_PFIFO_CACHE1_STATUS:
	case NV_PFIFO_RUNOUT_STATUS:
		// read-only
		break;

	default:
		REG_PFIFO(addr) = value;
	}
}

template<bool log, engine_enabled enabled>
uint32_t pfifo::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value = REG_PFIFO(addr);

	if constexpr (log) {
		log_read(addr, value);
	}

	return value;
}

template<bool log, engine_enabled enabled>
uint8_t pfifo::read8(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t addr_base = addr & ~3;
	uint32_t addr_offset = (addr & 3) << 3;
	uint32_t value32 = read32<false, on>(addr_base);
	uint8_t value = uint8_t((value32 & (0xFF << addr_offset)) >> addr_offset);

	if constexpr (log) {
		log_read(addr_base, value);
	}

	return value;
}

void
pfifo::pusher()
{
	if ((
		((REG_PFIFO(NV_PFIFO_CACHE1_PUSH0) & NV_PFIFO_CACHE1_PUSH0_ACCESS) << 1) |
		(REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) & (NV_PFIFO_CACHE1_DMA_PUSH_ACCESS | NV_PFIFO_CACHE1_DMA_PUSH_STATUS))
		) ^
		(NV_PFIFO_CACHE1_DMA_PUSH_ACCESS | (NV_PFIFO_CACHE1_PUSH0_ACCESS << 1))) {
		// Pusher is either disabled or suspended, so don't do anything
		return;
	}

	const auto &err_handler = [this](const char *msg, uint32_t code) {
		logger_en(warn, msg);
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE) |= (code << 29); // set error code
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) |= NV_PFIFO_CACHE1_DMA_PUSH_STATUS; // suspend pusher
		REG_PFIFO(NV_PFIFO_INTR_0) |= NV_PFIFO_INTR_0_DMA_PUSHER; // raise pusher interrupt
		m_machine->get<pmc>().update_irq();
		};

	// We are running, so set the busy flag
	REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) |= NV_PFIFO_CACHE1_DMA_PUSH_STATE;

	uint32_t curr_pb_get = REG_PFIFO(NV_PFIFO_CACHE1_DMA_GET) & ~3;
	uint32_t curr_pb_put = REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUT) & ~3;
	// Find the address of the new pb entries from the pb object
	dma_obj pb_obj = m_machine->get<nv2a>().get_dma_obj((REG_PFIFO(NV_PFIFO_CACHE1_DMA_INSTANCE) & NV_PFIFO_CACHE1_DMA_INSTANCE_ADDRESS) << 4);

	// Process all entries until the fifo is empty
	while (curr_pb_get != curr_pb_put) {
		if (curr_pb_get >= pb_obj.limit) {
			err_handler("Pusher error: curr_pb_get >= pb_obj.limit", NV_PFIFO_CACHE1_DMA_STATE_ERROR_PROTECTION); // set mem fault error
			break;
		}
		uint8_t *pb_addr = m_ram + pb_obj.target_addr + curr_pb_get; // ram host base addr + pb base addr + pb offset
		uint32_t pb_entry = *(uint32_t *)pb_addr;
		curr_pb_get += 4;

		uint32_t mthd_cnt = REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE) & NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT; // parameter count of method
		if (mthd_cnt) {
			// A method is already being processed, so the following words must be its parameters

			REG_PFIFO(NV_PFIFO_CACHE1_DMA_DATA_SHADOW) = pb_entry; // save in shadow reg the current entry

			uint32_t cache1_put = REG_PFIFO(NV_PFIFO_CACHE1_PUT) & 0x1FC;
			uint32_t dma_state = REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE);
			uint32_t mthd_type = dma_state & NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE; // method type
			uint32_t mthd = dma_state & NV_PFIFO_CACHE1_DMA_STATE_METHOD; // the actual method specified
			uint32_t mthd_subchan = dma_state & NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL; // the bound subchannel

			// Add the method and its parameter to cache1
			uint32_t method_entry = mthd_type | mthd | mthd_subchan;
			REG_PFIFO(NV_PFIFO_CACHE1_METHOD(cache1_put >> 2)) = method_entry;
			REG_PFIFO(NV_PFIFO_CACHE1_DATA(cache1_put >> 2)) = pb_entry;

			// Update dma state
			if (mthd_type == 0) {
				dma_state &= ~NV_PFIFO_CACHE1_DMA_STATE_METHOD;
				dma_state |= ((mthd + 4) >> 2);
			}
			mthd_cnt--;
			dma_state &= ~NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT;
			dma_state |= (mthd_cnt << 18);
			REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE) = dma_state;
			REG_PFIFO(NV_PFIFO_CACHE1_DMA_DCOUNT)++;

			// TODO: this should now either call or notify the puller that there's a new entry in cache1
			nxbx_fatal("Puller not implemented");
			break;
		}
		else {
			// No methods is currently active, so this must be a new one
			REG_PFIFO(NV_PFIFO_CACHE1_DMA_RSVD_SHADOW) = pb_entry; // save in shadow reg the current entry

			if ((pb_entry & 0xE0000003) == 0x20000000) {
				// old jump (nv4+) -> save current pb get addr and jump to the specified addr
				// 001JJJJJJJJJJJJJJJJJJJJJJJJJJJ00 -> J: jump addr
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW) = curr_pb_get;
				curr_pb_get = pb_entry & 0x1FFFFFFF;
			}
			else if ((pb_entry & 3) == 1) {
				// jump (nv1a+) -> same as old jump, but with a different method encoding
				// JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ01 -> J: jump addr
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW) = curr_pb_get;
				curr_pb_get = pb_entry & 0xFFFFFFFC;
			}
			else if ((pb_entry & 3) == 2) {
				// call (nv1a+) -> save current pb get addr and calls the routine at the specified addr
				// JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ10 -> J: call addr
				if (REG_PFIFO(NV_PFIFO_CACHE1_DMA_SUBROUTINE) & 1) {
					err_handler("Pusher error: call command while another subroutine is already active", NV_PFIFO_CACHE1_DMA_STATE_ERROR_CALL); // set call error
					break;
				}
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_SUBROUTINE) = curr_pb_get | 1;
				curr_pb_get = pb_entry & 0xFFFFFFFC;
			}
			else if (pb_entry == 0x00020000) {
				// return (nv1a+) -> restore pb get addr from subroutine return addr saved with a previous call
				// 00000000000000100000000000000000
				if ((REG_PFIFO(NV_PFIFO_CACHE1_DMA_SUBROUTINE) & 1) == 0) {
					err_handler("Pusher error: return command while subroutine is not active", NV_PFIFO_CACHE1_DMA_STATE_ERROR_RETURN); // set return error
					break;
				}
				curr_pb_get = REG_PFIFO(NV_PFIFO_CACHE1_DMA_SUBROUTINE) & ~3;
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_SUBROUTINE) = 0;
			}
			else if (uint32_t value = pb_entry & 0xE0030003; (value == 0) // increasing methods
				|| (value == 0x40000000)) { // non-increasing methods
				// Specify an new method
				// 00/10CCCCCCCCCCC00SSSMMMMMMMMMMM00 -> C: method count, S: subchannel, M: method
				uint32_t mthd_state = value == 0 ? 0 : 1;
				mthd_state |= ((pb_entry & NV_PFIFO_CACHE1_DMA_STATE_METHOD) | (pb_entry & NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL)
					| (pb_entry & NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT));
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE) = mthd_state;
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_DCOUNT) = 0;
			}
			else {
				err_handler("Pusher error: encountered unrecognized command", NV_PFIFO_CACHE1_DMA_STATE_ERROR_RESERVED_CMD); // set invalid command error
				break;
			}
		}
	}

	REG_PFIFO(NV_PFIFO_CACHE1_DMA_GET) = curr_pb_get;

	// We are done with processing, so clear the busy flag
	REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) &= ~NV_PFIFO_CACHE1_DMA_PUSH_STATE;
}

void
pfifo::puller()
{
	// TODO
}

void
pfifo::log_read(uint32_t addr, uint32_t value)
{
	const auto it = m_regs_info.find(addr & ~3);
	if (it != m_regs_info.end()) {
		logger<log_lv::debug, log_module::pfifo, false>("Read at %s (0x%08X) of value 0x%08X", it->second.c_str(), addr, value);
	}
	else {
		if (util::in_range(addr, NV_PFIFO_CACHE1_METHOD(0), NV_PFIFO_CACHE1_DATA(127) + 3)) {
			bool is_data = addr & 7;
			logger<log_lv::debug, log_module::pfifo, false>("Read at %s %u (0x%08X) of value 0x%08X", is_data ? "NV_PFIFO_CACHE1_DATA" : "NV_PFIFO_CACHE1_METHOD",
				is_data ? (addr - NV_PFIFO_CACHE1_DATA(0)) >> 3 : (addr - NV_PFIFO_CACHE1_METHOD(0)) >> 3, addr, value);
		}
		else {
			logger<log_lv::debug, log_module::pfifo, false>("Read at UNKNOWN + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PFIFO_BASE, addr, value);
		}
	}
}

void
pfifo::log_write(uint32_t addr, uint32_t value)
{
	const auto it = m_regs_info.find(addr & ~3);
	if (it != m_regs_info.end()) {
		logger<log_lv::debug, log_module::pfifo, false>("Write at %s (0x%08X) of value 0x%08X", it->second.c_str(), addr, value);
	}
	else {
		if (util::in_range(addr, NV_PFIFO_CACHE1_METHOD(0), NV_PFIFO_CACHE1_DATA(127) + 3)) {
			bool is_data = addr & 7;
			logger<log_lv::debug, log_module::pfifo, false>("Write at %s %u (0x%08X) of value 0x%08X", is_data ? "NV_PFIFO_CACHE1_DATA" : "NV_PFIFO_CACHE1_METHOD",
				is_data ? (addr - NV_PFIFO_CACHE1_DATA(0)) >> 3 : (addr - NV_PFIFO_CACHE1_METHOD(0)) >> 3, addr, value);
		}
		else {
			logger<log_lv::debug, log_module::pfifo, false>("Write at UNKNOWN + 0x%08X (0x%08X) of value 0x%08X", addr - NV_PFIFO_BASE, addr, value);
		}
	}
}

template<bool is_write, typename T>
auto pfifo::get_io_func(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pfifo, uint32_t, &pfifo::write32<true, on>, big> : nv2a_write<pfifo, uint32_t, &pfifo::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pfifo, uint32_t, &pfifo::write32<false, on>, big> : nv2a_write<pfifo, uint32_t, &pfifo::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pfifo, uint32_t, &pfifo::write32<false, off>, big>;
		}
	}
	else {
		if constexpr (sizeof(T) == 1) {
			if (enabled) {
				if (log) {
					return is_be ? nv2a_read<pfifo, uint8_t, &pfifo::read8<true, on>, big> : nv2a_read<pfifo, uint8_t, &pfifo::read8<true, on>, le>;
				}
				else {
					return is_be ? nv2a_read<pfifo, uint8_t, &pfifo::read8<false, on>, big> : nv2a_read<pfifo, uint8_t, &pfifo::read8<false, on>, le>;
				}
			}
			else {
				return nv2a_read<pfifo, uint8_t, &pfifo::read8<false, off>, big>;
			}
		}
		else {
			if (enabled) {
				if (log) {
					return is_be ? nv2a_read<pfifo, uint32_t, &pfifo::read32<true, on>, big> : nv2a_read<pfifo, uint32_t, &pfifo::read32<true, on>, le>;
				}
				else {
					return is_be ? nv2a_read<pfifo, uint32_t, &pfifo::read32<false, on>, big> : nv2a_read<pfifo, uint32_t, &pfifo::read32<false, on>, le>;
				}
			}
			else {
				return nv2a_read<pfifo, uint32_t, &pfifo::read32<false, off>, big>;
			}
		}
	}
}

bool
pfifo::update_io(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_machine->get<pmc>().engine_enabled & NV_PMC_ENABLE_PFIFO;
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFIFO_BASE, NV_PFIFO_SIZE, false,
		{
			.fnr8 = get_io_func<false, uint8_t>(log, enabled, is_be),
			.fnr32 = get_io_func<false, uint32_t>(log, enabled, is_be),
			.fnw32 = get_io_func<true, uint32_t>(log, enabled, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pfifo::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	REG_PFIFO(NV_PFIFO_CACHE1_STATUS) = NV_PFIFO_CACHE1_STATUS_LOW_MARK;
	REG_PFIFO(NV_PFIFO_RUNOUT_STATUS) = NV_PFIFO_RUNOUT_STATUS_LOW_MARK;
	// Values dumped from a Retail 1.0 xbox
	REG_PFIFO(NV_PFIFO_RAMHT) = 0x00000100;
	REG_PFIFO(NV_PFIFO_RAMFC) = 0x008A0110;
	REG_PFIFO(NV_PFIFO_RAMRO) = 0x00000114;
}

bool
pfifo::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	m_ram = get_ram_ptr(m_machine->get<cpu_t *>());
	return true;
}
