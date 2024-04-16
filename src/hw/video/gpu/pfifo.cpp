// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"
#include <functional>

#define MODULE_NAME pfifo


template<bool log, bool enabled>
void pfifo::write(uint32_t addr, const uint32_t data)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		log_io_write();
	}

	uint32_t addr_off = (addr - NV_PFIFO_BASE) >> 2;
	switch (addr)
	{
	case NV_PFIFO_INTR_0:
		regs[addr_off] &= ~data;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PFIFO_INTR_EN_0:
		regs[addr_off] = data;
		m_machine->get<pmc>().update_irq();
		break;

	case NV_PFIFO_CACHE1_DMA_PUSH:
		// Mask out read-only bits
		regs[addr_off] = (data & ~(NV_PFIFO_CACHE1_DMA_PUSH_STATE_MASK | NV_PFIFO_CACHE1_DMA_PUSH_BUFFER_MASK));
		break;

	case NV_PFIFO_CACHE1_DMA_PUT:
		regs[addr_off] = data;
		signal++;
		signal.notify_one();
		break;

	case NV_PFIFO_CACHE1_DMA_GET:
		regs[addr_off] = data;
		signal++;
		signal.notify_one();
		break;

	default:
		regs[addr_off] = data;
	}
}

template<bool log, bool enabled>
uint32_t pfifo::read(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t addr_off = (addr - NV_PFIFO_BASE) >> 2;
	uint32_t value = regs[addr_off];

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

void
pfifo::pusher(auto &err_handler)
{
	if ((
		((regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_PUSH0)] & NV_PFIFO_CACHE1_PUSH0_ACCESS_MASK) << 1) |
		(regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_PUSH)] & (NV_PFIFO_CACHE1_DMA_PUSH_ACCESS_MASK | NV_PFIFO_CACHE1_DMA_PUSH_STATUS_MASK))
		) ^
		(NV_PFIFO_CACHE1_DMA_PUSH_ACCESS_MASK | (NV_PFIFO_CACHE1_PUSH0_ACCESS_MASK << 1))) {
		// Pusher is either disabled or suspended, so don't do anything
		return;
	}

	// We are running, so set the busy flag
	regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_PUSH)] |= NV_PFIFO_CACHE1_DMA_PUSH_STATE_MASK;

	uint32_t curr_pb_get = regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_GET)] & ~3;
	uint32_t curr_pb_put = regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_PUT)] & ~3;
	// Find the address of the new pb entries from the pb object
	dma_obj pb_obj = m_machine->get<nv2a>().get_dma_obj((regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_INSTANCE)] & NV_PFIFO_CACHE1_DMA_INSTANCE_ADDRESS_MASK) << 4);

	// Process all entries until the fifo is empty
	while (curr_pb_get != curr_pb_put) {
		if (curr_pb_get >= pb_obj.limit) {
			regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)] |= (NV_PFIFO_CACHE1_DMA_STATE_ERROR_PROTECTION << 29); // set mem fault error
			err_handler("Pusher error: curr_pb_get >= pb_obj.limit");
			break;
		}
		uint8_t *pb_addr = m_ram + pb_obj.target_addr + curr_pb_get; // ram host base addr + pb base addr + pb offset
		uint32_t pb_entry = *(uint32_t *)pb_addr;
		curr_pb_get += 4;

		uint32_t mthd_cnt = regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)] & NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT_MASK; // parameter count of method
		if (mthd_cnt) {
			// A method is already being processed, so the following words must be its parameters

			regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_DATA_SHADOW)] = pb_entry; // save in shadow reg the current entry

			uint32_t cache1_put = regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_PUT)] & 0x1FC;
			uint32_t dma_state = regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)];
			uint32_t mthd_type = dma_state & NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE_MASK; // method type
			uint32_t mthd = dma_state & NV_PFIFO_CACHE1_DMA_STATE_METHOD_MASK; // the actual method specified
			uint32_t mthd_subchan = dma_state & NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL_MASK; // the bound subchannel

			// Add the method and its parameter to cache1
			uint32_t method_entry = mthd_type | mthd | mthd_subchan;
			regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_METHOD(cache1_put >> 2))] = method_entry;
			regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DATA(cache1_put >> 2))] = pb_entry;

			// Update dma state
			if (mthd_type == 0) {
				dma_state &= ~NV_PFIFO_CACHE1_DMA_STATE_METHOD_MASK;
				dma_state |= ((mthd + 4) >> 2);
			}
			mthd_cnt--;
			dma_state &= ~NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT_MASK;
			dma_state |= (mthd_cnt << 18);
			regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)] = dma_state;
			regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_DCOUNT)]++;

			// TODO: this should now either call or notify the puller that there's a new entry in cache1
			nxbx_fatal("Puller not implemented");
			break;
		}
		else {
			// No methods is currently active, so this must be a new one
			regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_RSVD_SHADOW)] = pb_entry; // save in shadow reg the current entry

			if ((pb_entry & 0xE0000003) == 0x20000000) {
				// old jump (nv4+) -> save current pb get addr and jump to the specified addr
				// 001JJJJJJJJJJJJJJJJJJJJJJJJJJJ00 -> J: jump addr
				regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW)] = curr_pb_get;
				curr_pb_get = pb_entry & 0x1FFFFFFF;
			}
			else if ((pb_entry & 3) == 1) {
				// jump (nv1a+) -> same as old jump, but with a different method encoding
				// JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ01 -> J: jump addr
				regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW)] = curr_pb_get;
				curr_pb_get = pb_entry & 0xFFFFFFFC;
			}
			else if ((pb_entry & 3) == 2) {
				// call (nv1a+) -> save current pb get addr and calls the routine at the specified addr
				// JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ10 -> J: call addr
				if (regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_SUBROUTINE)] & 1) {
					regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)] |= (NV_PFIFO_CACHE1_DMA_STATE_ERROR_CALL << 29); // set call error
					err_handler("Pusher error: call command while another subroutine is already active");
					break;
				}
				regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_SUBROUTINE)] = curr_pb_get | 1;
				curr_pb_get = pb_entry & 0xFFFFFFFC;
			}
			else if (pb_entry == 0x00020000) {
				// return (nv1a+) -> restore pb get addr from subroutine return addr saved with a previous call
				// 00000000000000100000000000000000
				if ((regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_SUBROUTINE)] & 1) == 0) {
					regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)] |= (NV_PFIFO_CACHE1_DMA_STATE_ERROR_RETURN << 29); // set return error
					err_handler("Pusher error: return command while subroutine is not active");
					break;
				}
				curr_pb_get = regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_SUBROUTINE)] & ~3;
				regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_SUBROUTINE)] = 0;
			}
			else if (uint32_t value = pb_entry & 0xE0030003; (value == 0) // increasing methods
				|| (value == 0x40000000)) { // non-increasing methods
				// Specify an new method
				// 00/10CCCCCCCCCCC00SSSMMMMMMMMMMM00 -> C: method count, S: subchannel, M: method
				uint32_t mthd_state = value == 0 ? 0 : 1;
				mthd_state |= ((pb_entry & NV_PFIFO_CACHE1_DMA_STATE_METHOD_MASK) | (pb_entry & NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL_MASK)
					| (pb_entry & NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT_MASK));
				regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)] = mthd_state;
				regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_DCOUNT)] = 0;
			}
			else {
				regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_STATE)] |= (NV_PFIFO_CACHE1_DMA_STATE_ERROR_RESERVED_CMD << 29); // set invalid command error
				err_handler("Pusher error: encountered unrecognized command");
				break;
			}
		}
	}

	regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_GET)] = curr_pb_get;

	// We are done with processing, so clear the busy flag
	regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_PUSH)] &= ~NV_PFIFO_CACHE1_DMA_PUSH_STATE_MASK;
}

void
pfifo::puller()
{
	// TODO
}

void
pfifo::worker(std::stop_token stok)
{
	// This function is called in a separate thread, and acts as the pfifo pusher and puller

	// This lambda is called when the pusher encounters an error
	const auto lambda = [this](const char *msg) {
		regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_DMA_PUSH)] |= NV_PFIFO_CACHE1_DMA_PUSH_STATUS_MASK; // suspend pusher
		// Currently disabled, because it's not thread safe yet
#if 0
		regs[REGS_PFIFO_idx(NV_PFIFO_INTR_0)] |= NV_PFIFO_INTR_0_DMA_PUSHER; // raise pusher interrupt
		m_machine->get<pmc>().update_irq();
#endif
		nxbx_fatal(msg);
		};

	while (true) {
		// Wait until there's some work to do
		signal.wait(0);

		if (stok.stop_requested()) [[unlikely]] {
			return;
		}

		pusher(lambda);

		signal--;
	}
}

template<bool is_write>
auto pfifo::get_io_func(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pfifo, uint32_t, &pfifo::write<true, true>, true> : nv2a_write<pfifo, uint32_t, &pfifo::write<true>>;
			}
			else {
				return is_be ? nv2a_write<pfifo, uint32_t, &pfifo::write<false, true>, true> : nv2a_write<pfifo, uint32_t, &pfifo::write<false>>;
			}
		}
		else {
			return nv2a_write<pfifo, uint32_t, &pfifo::write<false, false>>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pfifo, uint32_t, &pfifo::read<true, true>, true> : nv2a_read<pfifo, uint32_t, &pfifo::read<true>>;
			}
			else {
				return is_be ? nv2a_read<pfifo, uint32_t, &pfifo::read<false, true>, true> : nv2a_read<pfifo, uint32_t, &pfifo::read<false>>;
			}
		}
		else {
			return nv2a_read<pfifo, uint32_t, &pfifo::read<false, false>>;
		}
	}
}

bool
pfifo::update_io(bool is_update)
{
	bool log = module_enabled();
	bool enabled = m_machine->get<pmc>().engine_enabled & NV_PMC_ENABLE_PFIFO;
	bool is_be = m_machine->get<pmc>().endianness & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PFIFO_BASE, NV_PFIFO_SIZE, false,
		{
			.fnr32 = get_io_func<false>(log, enabled, is_be),
			.fnw32 = get_io_func<true>(log, enabled, is_be)
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
	std::fill(std::begin(regs), std::end(regs), 0);
	regs[REGS_PFIFO_idx(NV_PFIFO_CACHE1_STATUS)] = NV_PFIFO_CACHE1_STATUS_LOW_MARK_MASK;
	// Values dumped from a Retail 1.0 xbox
	regs[REGS_PFIFO_idx(NV_PFIFO_RAMHT)] = 0x00000100;
	regs[REGS_PFIFO_idx(NV_PFIFO_RAMFC)] = 0x008A0110;
	regs[REGS_PFIFO_idx(NV_PFIFO_RAMRO)] = 0x00000114;
}

bool
pfifo::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	signal = 0;
	m_ram = get_ram_ptr(m_machine->get<cpu_t *>());
	jthr = std::jthread(std::bind_front(&pfifo::worker, this));
	return true;
}

void
pfifo::deinit()
{
	jthr.request_stop();
	signal++;
	signal.notify_one();
	jthr.join();
}
