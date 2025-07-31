// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include "nv2a_defs.hpp"

#define NV_PFIFO 0x00002000
#define NV_PFIFO_BASE (NV2A_REGISTER_BASE + NV_PFIFO)
#define NV_PFIFO_SIZE 0x2000
#define REGS_PFIFO_idx(x) ((x - NV_PFIFO_BASE) >> 2)
#define REG_PFIFO(r) (m_regs[REGS_PFIFO_idx(r)])

#define NV_PFIFO_INTR_0 (NV2A_REGISTER_BASE + 0x00002100) // Pending pfifo interrupts. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PFIFO_INTR_0_DMA_PUSHER (1 << 12)
#define NV_PFIFO_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00002140) // Enable/disable pfifo interrupts
#define NV_PFIFO_RAMHT (NV2A_REGISTER_BASE + 0x00002210) // Contains the base address and size of ramht in ramin
#define NV_PFIFO_RAMFC (NV2A_REGISTER_BASE + 0x00002214) // Contains the base address and size of ramfc in ramin
#define NV_PFIFO_RAMRO (NV2A_REGISTER_BASE + 0x00002218) // Contains the base address and size of ramro in ramin
#define NV_PFIFO_RUNOUT_STATUS (NV2A_REGISTER_BASE + 0x00002400) // Status of ramro in ramin
#define NV_PFIFO_RUNOUT_STATUS_LOW_MARK (1 << 4) // empty=1
#define NV_PFIFO_MODE (NV2A_REGISTER_BASE + 0x00002504) // Indicates the submission mode, one bit for each channel
#define NV_PFIFO_MODE_CHANNEL(id) (1 << (id)) // pio=0, dma=1
#define NV_PFIFO_CACHE1_PUSH0 (NV2A_REGISTER_BASE + 0x00003200) // Enable/disable pusher access to cache1
#define NV_PFIFO_CACHE1_PUSH0_ACCESS (1 << 0) // enabled=1
#define NV_PFIFO_CACHE1_PUSH1 (NV2A_REGISTER_BASE + 0x00003204) // The currently active channel id and the mode it uses (cache1)
#define NV_PFIFO_CACHE1_PUSH1_CHID 0x1F
#define NV_PFIFO_CACHE1_PUSH1_MODE (1 << 8) // 1=dma
#define NV_PFIFO_CACHE1_PUT (NV2A_REGISTER_BASE + 0x00003210) // The front pointer of cache1
#define NV_PFIFO_CACHE1_STATUS (NV2A_REGISTER_BASE + 0x00003214) // Empty/full flag of cache1
#define NV_PFIFO_CACHE1_STATUS_LOW_MARK (1 << 4) // 1=empty
#define NV_PFIFO_CACHE1_STATUS_HIGH_MARK (1 << 8) // 1=full
#define NV_PFIFO_CACHE1_DMA_PUSH (NV2A_REGISTER_BASE + 0x00003220) // Status bits of the pusher
#define NV_PFIFO_CACHE1_DMA_PUSH_ACCESS (1 << 0) // enabled=1
#define NV_PFIFO_CACHE1_DMA_PUSH_STATE (1 << 4) // busy=1
#define NV_PFIFO_CACHE1_DMA_PUSH_BUFFER (1 << 8)
#define NV_PFIFO_CACHE1_DMA_PUSH_STATUS (1 << 12) // suspended=1
#define NV_PFIFO_CACHE1_DMA_PUSH_ACQUIRE (1 << 16)
#define NV_PFIFO_CACHE1_DMA_FETCH (NV2A_REGISTER_BASE + 0x00003224) // Dma fetch flags
#define NV_PFIFO_CACHE1_DMA_FETCH_ENDIAN (1 << 31) // 1=big
#define NV_PFIFO_CACHE1_DMA_STATE (NV2A_REGISTER_BASE + 0x00003228) // Current pb processing state of the pusher
#define NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE (1 << 0) // non-increasing=1
#define NV_PFIFO_CACHE1_DMA_STATE_METHOD 0x00001FFC
#define NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL 0x0000E000
#define NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT 0x1FFC0000
#define NV_PFIFO_CACHE1_DMA_STATE_ERROR 0xE0000000
#define NV_PFIFO_CACHE1_DMA_STATE_ERROR_CALL 0x00000001
#define NV_PFIFO_CACHE1_DMA_STATE_ERROR_RETURN 0x00000003
#define NV_PFIFO_CACHE1_DMA_STATE_ERROR_RESERVED_CMD 0x00000004
#define NV_PFIFO_CACHE1_DMA_STATE_ERROR_PROTECTION 0x00000006
#define NV_PFIFO_CACHE1_DMA_INSTANCE (NV2A_REGISTER_BASE + 0x0000322C) // The addr of the dma pb object
#define NV_PFIFO_CACHE1_DMA_INSTANCE_ADDRESS 0xFFFF
#define NV_PFIFO_CACHE1_DMA_PUT (NV2A_REGISTER_BASE + 0x00003240) // The front pointer of the active pb fifo
#define NV_PFIFO_CACHE1_DMA_GET (NV2A_REGISTER_BASE + 0x00003244) // The back pointer of the active pb fifo
#define NV_PFIFO_CACHE1_REF (NV2A_REGISTER_BASE + 0x00003248) // reference count of the active pb (set when the REF_CNT method is executed)
#define NV_PFIFO_CACHE1_DMA_SUBROUTINE (NV2A_REGISTER_BASE + 0x0000324C)  // copy of NV_PFIFO_CACHE1_DMA_GET before the call + subroutine active flag
#define NV_PFIFO_CACHE1_GET (NV2A_REGISTER_BASE + 0x00003270) // The back pointer of cache1
#define NV_PFIFO_CACHE1_DMA_DCOUNT (NV2A_REGISTER_BASE + 0x000032A0) // the number of parameters that have being processed for the current method
#define NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW (NV2A_REGISTER_BASE + 0x000032A4) // copy of NV_PFIFO_CACHE1_DMA_GET before the jump
#define NV_PFIFO_CACHE1_DMA_RSVD_SHADOW (NV2A_REGISTER_BASE + 0x000032A8) // copy of pb entry when new method is processed
#define NV_PFIFO_CACHE1_DMA_DATA_SHADOW (NV2A_REGISTER_BASE + 0x000032AC) // copy of pb entry when the method's parameters are being processed
#define NV_PFIFO_CACHE1_METHOD(i) (NV2A_REGISTER_BASE + 0x00003800 + (i) * 8) // cache1 register array of 128 entries (caches methods)
#define NV_PFIFO_CACHE1_DATA(i) (NV2A_REGISTER_BASE + 0x00003804 + (i) * 8) // cache1 register array of 128 entries (caches parameters)


class machine;
class nv2a;
class pmc;
class puser;
enum engine_enabled : int;

class pfifo {
public:
	pfifo(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io() { update_io(true); }
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	uint8_t read8(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);

private:
	void log_read(uint32_t addr, uint32_t value);
	void log_write(uint32_t addr, uint32_t value);
	bool update_io(bool is_update);
	template<bool is_write, typename T>
	auto get_io_func(bool log, bool enabled, bool is_be);
	void pusher();
	void puller();

	friend class nv2a;
	friend class pmc;
	friend class puser;
	machine *const m_machine;
	uint8_t *m_ram;
	// registers
	uint32_t m_regs[NV_PFIFO_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PFIFO_INTR_0, "NV_PFIFO_INTR_0" },
		{ NV_PFIFO_INTR_EN_0, "NV_PFIFO_INTR_EN_0" },
		{ NV_PFIFO_RAMHT, "NV_PFIFO_RAMHT" },
		{ NV_PFIFO_RAMFC, "NV_PFIFO_RAMFC" },
		{ NV_PFIFO_RAMRO, "NV_PFIFO_RAMRO" },
		{ NV_PFIFO_RUNOUT_STATUS, "NV_PFIFO_RUNOUT_STATUS" },
		{ NV_PFIFO_MODE, "NV_PFIFO_MODE" },
		{ NV_PFIFO_CACHE1_PUSH0, "NV_PFIFO_CACHE1_PUSH0" },
		{ NV_PFIFO_CACHE1_PUSH1, "NV_PFIFO_CACHE1_PUSH1" },
		{ NV_PFIFO_CACHE1_PUT, "NV_PFIFO_CACHE1_PUT" },
		{ NV_PFIFO_CACHE1_STATUS, "NV_PFIFO_CACHE1_STATUS" },
		{ NV_PFIFO_CACHE1_DMA_PUSH, "NV_PFIFO_CACHE1_DMA_PUSH" },
		{ NV_PFIFO_CACHE1_DMA_FETCH, "NV_PFIFO_CACHE1_DMA_FETCH" },
		{ NV_PFIFO_CACHE1_DMA_STATE, "NV_PFIFO_CACHE1_DMA_STATE" },
		{ NV_PFIFO_CACHE1_DMA_INSTANCE, "NV_PFIFO_CACHE1_DMA_INSTANCE" },
		{ NV_PFIFO_CACHE1_DMA_PUT, "NV_PFIFO_CACHE1_DMA_PUT" },
		{ NV_PFIFO_CACHE1_DMA_GET, "NV_PFIFO_CACHE1_DMA_GET" },
		{ NV_PFIFO_CACHE1_REF, "NV_PFIFO_CACHE1_REF" },
		{ NV_PFIFO_CACHE1_DMA_SUBROUTINE, "NV_PFIFO_CACHE1_DMA_SUBROUTINE" },
		{ NV_PFIFO_CACHE1_GET, "NV_PFIFO_CACHE1_GET" },
		{ NV_PFIFO_CACHE1_DMA_DCOUNT, "NV_PFIFO_CACHE1_DMA_DCOUNT" },
		{ NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW, "NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW" },
		{ NV_PFIFO_CACHE1_DMA_RSVD_SHADOW, "NV_PFIFO_CACHE1_DMA_RSVD_SHADOW" },
		{ NV_PFIFO_CACHE1_DMA_DATA_SHADOW, "NV_PFIFO_CACHE1_DMA_DATA_SHADOW" },
		{ NV_PFIFO_CACHE1_METHOD(0), "NV_PFIFO_CACHE1_METHOD + 0"},
		{ NV_PFIFO_CACHE1_DATA(0), "NV_PFIFO_CACHE1_DATA + 0"}
	};
};
