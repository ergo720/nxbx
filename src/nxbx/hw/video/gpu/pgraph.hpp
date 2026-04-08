// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2025 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PGRAPH 0x00400000
#define NV_PGRAPH_BASE (NV2A_REGISTER_BASE + NV_PGRAPH)
#define NV_PGRAPH_SIZE 0x2000
#define REGS_PGRAPH_idx(x) ((x - NV_PGRAPH_BASE) >> 2)
#define REG_PGRAPH(r) (m_regs[REGS_PGRAPH_idx(r)])

#define NV_PGRAPH_DEBUG_3 (NV2A_REGISTER_BASE + 0x0040008C) // debug flags 3
#define NV_PGRAPH_DEBUG_3_HW_CONTEXT_SWITCH (1 << 2) // hw context switch, enabled=1
#define NV_PGRAPH_INTR (NV2A_REGISTER_BASE + 0x00400100) // Pending pgraph interrupts. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PGRAPH_INTR_CONTEXT_SWITCH (1 << 12)
#define NV_PGRAPH_INTR_EN (NV2A_REGISTER_BASE + 0x00400140) // Enable/disable pgraph interrupts
#define NV_PGRAPH_CTX_CONTROL (NV2A_REGISTER_BASE + 0x00400144) // misc channel state
#define NV_PGRAPH_CTX_CONTROL_CHID (1 << 16) // valid channel=1
#define NV_PGRAPH_CTX_USER (NV2A_REGISTER_BASE + 0x00400148) // 3d channel state
#define NV_PGRAPH_CTX_USER_CHID (0x1F << 24) // channel in use
#define NV_PGRAPH_CTX_SWITCH1 (NV2A_REGISTER_BASE + 0x0040014C) // object context bound to subchannel specified by current method being accepted 1
#define NV_PGRAPH_CTX_SWITCH2 (NV2A_REGISTER_BASE + 0x00400150) // object context bound to subchannel specified by current method being accepted 2
#define NV_PGRAPH_CTX_SWITCH3 (NV2A_REGISTER_BASE + 0x00400154) // object context bound to subchannel specified by current method being accepted 3
#define NV_PGRAPH_CTX_SWITCH4 (NV2A_REGISTER_BASE + 0x00400158) // object context bound to subchannel specified by current method being accepted 4
#define NV_PGRAPH_CTX_SWITCH5 (NV2A_REGISTER_BASE + 0x0040015C) // object context bound to subchannel specified by current method being accepted 5
#define NV_PGRAPH_CTX_CACHE1(i) (NV2A_REGISTER_BASE + 0x00400160 + (i) * 4) // cached object context, one register for each subchannel 1
#define NV_PGRAPH_CTX_CACHE2(i) (NV2A_REGISTER_BASE + 0x00400180 + (i) * 4) // cached object context, one register for each subchannel 2
#define NV_PGRAPH_CTX_CACHE3(i) (NV2A_REGISTER_BASE + 0x004001A0 + (i) * 4) // cached object context, one register for each subchannel 3
#define NV_PGRAPH_CTX_CACHE4(i) (NV2A_REGISTER_BASE + 0x004001C0 + (i) * 4) // cached object context, one register for each subchannel 4
#define NV_PGRAPH_CTX_CACHE5(i) (NV2A_REGISTER_BASE + 0x004001E0 + (i) * 4) // cached object context, one register for each subchannel 5
#define NV_PGRAPH_TRAPPED_ADDR (NV2A_REGISTER_BASE + 0x00400704) // info on the exception that was triggered
#define NV_PGRAPH_TRAPPED_ADDR_MTHD 0x1FFC // method that faulted
#define NV_PGRAPH_TRAPPED_ADDR_SUBCH (7 << 16) // subchannel that faulted
#define NV_PGRAPH_TRAPPED_ADDR_CHID (0x1F << 20) // channel that faulted
#define NV_PGRAPH_FIFO (NV2A_REGISTER_BASE + 0x00400720) // Enable/disable pfifo access to pgraph
#define NV_PGRAPH_FIFO_ACCESS (1 << 0) // enabled=1
#define NV_PGRAPH_CHANNEL_CTX_POINTER (NV2A_REGISTER_BASE + 0x00400784) // address of graphics context
#define NV_PGRAPH_CHANNEL_CTX_POINTER_INST 0xFFFF // actual address is NV_PRAMIN_BASE + (value << 4)
#define NV_PGRAPH_CHANNEL_CTX_TRIGGER (NV2A_REGISTER_BASE + 0x00400788) // triggers graphics context (un)loading
#define NV_PGRAPH_CHANNEL_CTX_TRIGGER_READ_IN (1 << 0) // load context
#define NV_PGRAPH_CHANNEL_CTX_TRIGGER_WRITE_OUT (1 << 1) // unload context


class cpu;
class nv2a;

class pgraph
{
public:
	pgraph();
	~pgraph();
	bool init(cpu *cpu, nv2a *gpu);
	void deinit();
	void reset();
	void updateIo();
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);
	template<bool is_mthd_zero>
	void submitMethod(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t chid);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
