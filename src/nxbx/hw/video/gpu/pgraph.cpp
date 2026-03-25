// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2025 ergo720

#include "machine.hpp"
#include <cassert>

#define MODULE_NAME pgraph

#define SET_REG(reg, mask, val) (REG_PGRAPH(reg) &= ~(mask)) |= (val)


template<bool log, engine_enabled enabled>
void pgraph::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		m_graph_mtx.lock();
		nv2a_log_write();
		m_graph_mtx.unlock();
	}

	switch (addr)
	{
	case NV_PGRAPH_INTR:
		m_int_status &= ~value;
		if ((m_int_status & NV_PGRAPH_INTR_CONTEXT_SWITCH) == 0) {
			m_graph_mtx.lock();
			m_ctx_switch_trig.clear();
			m_graph_mtx.unlock();
			m_ctx_switch_trig.notify_one();
		}
		m_machine->invoke(&pmc::updateIrq);
		break;

	case NV_PGRAPH_INTR_EN:
		m_int_enabled = value;
		m_machine->invoke(&pmc::updateIrq);
		break;

	case NV_PGRAPH_TRAPPED_ADDR:
		// read-only
		break;

	case NV_PGRAPH_FIFO:
		m_fifo_access = value;
		break;

	case NV_PGRAPH_CHANNEL_CTX_TRIGGER:
		if (value & NV_PGRAPH_CHANNEL_CTX_TRIGGER_READ_IN) {
			m_graph_mtx.lock();
			uint32_t gr_ctx_addr = (REG_PGRAPH(NV_PGRAPH_CHANNEL_CTX_POINTER) & NV_PGRAPH_CHANNEL_CTX_POINTER_INST) << 4;
			uint32_t gr_ctx = m_machine->invoke(&pramin::read<uint32_t>, NV_PRAMIN_BASE + gr_ctx_addr);
			REG_PGRAPH(NV_PGRAPH_CTX_USER) = gr_ctx;
			m_graph_mtx.unlock();
		}
		break;

	default:
		m_graph_mtx.lock();
		REG_PGRAPH(addr) = value;
		m_graph_mtx.unlock();
	}
}

template<bool log, engine_enabled enabled>
uint32_t pgraph::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t value;

	switch (addr)
	{
	case NV_PGRAPH_INTR:
		value = m_int_status;
		break;

	case NV_PGRAPH_INTR_EN:
		value = m_int_enabled;
		break;

	case NV_PGRAPH_FIFO:
		value = m_fifo_access;
		break;

	default:
		m_graph_mtx.lock();
		value = REG_PGRAPH(addr);
		m_graph_mtx.unlock();
	}

	if constexpr (log) {
		m_graph_mtx.lock();
		nv2a_log_read();
		m_graph_mtx.unlock();
	}

	return value;
}

void pgraph::submitMethod(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t ctx_switch)
{
	// called from the fifo thread
	m_input_queue.emplace(mthd, param, subchan, ctx_switch); // blocks if the queue is full
	m_graph_has_work.test_and_set();
	m_graph_has_work.notify_one();
}

void pgraph::drainInputQueue()
{
	while (!m_input_queue.empty()) {
		InputQueueEntry elem;
		m_input_queue.pop(elem);
	}
}

void pgraph::graphHandler(std::stop_token stok)
{
	while (true) {
		// Wait until the puller pushes some methods
		m_graph_has_work.wait(false);
		std::unique_lock lock(m_graph_mtx);
		m_graph_has_work.clear();

		if (stok.stop_requested()) [[unlikely]] {
			return;
		}

		if ((m_fifo_access & NV_PGRAPH_FIFO_ACCESS) == 0) {
			// Fifo access to graph is disabled, so keep looping
			continue;
		}

		if (m_is_enabled == false) {
			// Need to check this too because fifo will keep submitting methods even when this engine is disabled
			continue;
		}

		InputQueueEntry elem;
		m_input_queue.pop(elem);

		if (elem.m_ctx_switch & CTX_SWITCH_STATUS) {
			// A context switch was requested, do it now
			assert(elem.m_mthd == 0);
			uint32_t target_chid = (elem.m_ctx_switch & CTX_SWITCH_CHID);
			bool valid_chid = REG_PGRAPH(NV_PGRAPH_CTX_CONTROL) & NV_PGRAPH_CTX_CONTROL_CHID;
			uint32_t curr_chid = (REG_PGRAPH(NV_PGRAPH_CTX_USER) & NV_PGRAPH_CTX_USER_CHID) >> 24;

			if (!valid_chid || (curr_chid != target_chid)) {
				if (REG_PGRAPH(NV_PGRAPH_DEBUG_3) & NV_PGRAPH_DEBUG_3_HW_CONTEXT_SWITCH) [[unlikely]] {
					// Should never happen on xbox
					nxbx_fatal("Hw context switch not implemented");
					return;
				}
				SET_REG(NV_PGRAPH_TRAPPED_ADDR, NV_PGRAPH_TRAPPED_ADDR_CHID, target_chid << 20); // write channel exception data
				m_int_status |= NV_PGRAPH_INTR_CONTEXT_SWITCH; // raise graph interrupt
				m_ctx_switch_trig.test_and_set();
				lock.unlock();
				m_machine->invoke(&pmc::updateIrq);
				m_ctx_switch_trig.wait(true); // wait until the title does the switch and clears the interrupt
				lock.lock();

				if (stok.stop_requested()) [[unlikely]] {
					return;
				}
			}
		}

		nxbx_fatal("Method 0x%08" PRIX32 ", subchannel %" PRIu32 ", parameter 0x%08" PRIX32 " not implemented", elem.m_mthd, elem.m_subchan, elem.m_param);
		return;
	}
}

template<bool is_write>
auto pgraph::get_io_func(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pgraph, uint32_t, &pgraph::write32<true, on>, big> : nv2a_write<pgraph, uint32_t, &pgraph::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pgraph, uint32_t, &pgraph::write32<false, on>, big> : nv2a_write<pgraph, uint32_t, &pgraph::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pgraph, uint32_t, &pgraph::write32<false, off>, big>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pgraph, uint32_t, &pgraph::read32<true, on>, big> : nv2a_read<pgraph, uint32_t, &pgraph::read32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_read<pgraph, uint32_t, &pgraph::read32<false, on>, big> : nv2a_read<pgraph, uint32_t, &pgraph::read32<false, on>, le>;
			}
		}
		else {
			return nv2a_read<pgraph, uint32_t, &pgraph::read32<false, off>, big>;
		}
	}
}

bool
pgraph::update_io(bool is_update)
{
	bool log = module_enabled();
	m_is_enabled = m_machine->invoke(&pmc::read32<false>, NV_PMC_ENABLE) & NV_PMC_ENABLE_PGRAPH;
	bool is_be = m_machine->invoke(&pmc::read32<false>, NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get_cpu(), NV_PGRAPH_BASE, NV_PGRAPH_SIZE, false,
		{
			.fnr32 = get_io_func<false>(log, m_is_enabled, is_be),
			.fnw32 = get_io_func<true>(log, m_is_enabled, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void pgraph::reset()
{
	m_int_status = 0;
	m_int_enabled = 0;
	m_fifo_access = 0;
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_graph_has_work.clear();
}

bool pgraph::init()
{
	reset();

	if (!update_io(false)) {
		return false;
	}

	m_jthr = std::jthread(std::bind_front(&pgraph::graphHandler, this));
	return true;
}

void pgraph::deinit()
{
	if (m_jthr.joinable()) {
		m_jthr.request_stop();
		m_ctx_switch_trig.clear();
		m_ctx_switch_trig.notify_one();
		m_graph_has_work.test_and_set();
		m_graph_has_work.notify_one();
		m_jthr.join();
	}
}
