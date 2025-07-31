// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "nv2a.hpp"
#include "machine.hpp"


bool
nv2a::init()
{
	if (!m_pmc.init()) {
		return false;
	}
	if (!m_pramdac.init()) {
		return false;
	}
	if (!m_pbus.init()) {
		return false;
	}
	if (!m_pfb.init()) {
		return false;
	}
	if (!m_pcrtc.init()) {
		return false;
	}
	if (!m_ptimer.init()) {
		return false;
	}
	if (!m_pramin.init()) {
		return false;
	}
	if (!m_pfifo.init()) {
		return false;
	}
	if (!m_pvga.init()) {
		return false;
	}
	if (!m_pvideo.init()) {
		return false;
	}
	if (!m_puser.init()) {
		return false;
	}
	if (!m_pgraph.init()) {
		return false;
	}
	return true;
}

uint64_t
nv2a::get_next_update_time(uint64_t now)
{
	return m_ptimer.get_next_alarm_time(now);
}

dma_obj
nv2a::get_dma_obj(uint32_t addr)
{
	/*
	A dma object has the following memory layout:
	base+0: flags -> 0:11 class type, 12:13 page table stuff, 16:17 mem type, 20:31 high 12 bits of target addr
	base+4: limit -> 0:31 addr limit for the resource at the target addr
	base+8: addr -> 12:31 low 20 bits of target addr
	*/

	// TODO: this should also consider the endianness bit of NV_PFIFO_CACHE1_DMA_FETCH
	uint32_t flags = m_pramin.read<uint32_t>(NV_PRAMIN_BASE + addr);
	uint32_t limit = m_pramin.read<uint32_t>(NV_PRAMIN_BASE + addr + 4);
	uint32_t addr_info = m_pramin.read<uint32_t>(NV_PRAMIN_BASE + addr + 8);

	return dma_obj{
		.class_type = flags & NV_DMA_CLASS,
		.mem_type = (flags & NV_DMA_TARGET) >> 16,
		.target_addr = (((flags & NV_DMA_ADJUST) >> 20) | (addr_info & NV_DMA_ADDRESS)) & (RAM_SIZE128 - 1),
		.limit = limit,
	};
}

void
nv2a::apply_log_settings()
{
	m_pmc.update_io();
	m_pcrtc.update_io();
	m_pramdac.update_io();
	m_ptimer.update_io();
	m_pfb.update_io();
	m_pbus.update_io();
	m_pramin.update_io();
	m_pfifo.update_io();
	m_pvga.update_io();
	m_pvideo.update_io();
	m_puser.update_io();
}
