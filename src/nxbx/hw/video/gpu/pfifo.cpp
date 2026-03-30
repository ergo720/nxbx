// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.h"
#include "pfifo.hpp"
#include "pmc.hpp"
#include "pramin.hpp"
#include "pgraph.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include "util.hpp"
#include <thread>
#include <mutex>
#include <coroutine>
#include <atomic>
#include <functional>
#include <cassert>
#include <bit>
#include <charconv>
#include <cinttypes>

#define MODULE_NAME pfifo


/** Private device implementation **/
class pfifo::Impl
{
public:
	bool init(cpu *cpu, nv2a *gpu);
	void deinit();
	void reset();
	void updateIo() { updateIo(true); }
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	uint8_t read8(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);

private:
	struct CoroFrame
	{
		struct promise_type
		{
			CoroFrame get_return_object()
			{
				return CoroFrame{ std::coroutine_handle<promise_type>::from_promise(*this) };
			}
			constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
			constexpr std::suspend_never final_suspend() const noexcept { return {}; }
			constexpr void return_void() const noexcept {}
			void unhandled_exception() { throw; } // rethrow the exception to terminate the fifo thread
		};
		explicit CoroFrame(std::coroutine_handle<promise_type> h) : m_handle(h) {}

		std::coroutine_handle<promise_type> m_handle;
	};
	struct RamhtElement
	{
		uint32_t m_handle; // handle of the object
		uint32_t m_instance; // addr of object inside ramin
		uint32_t m_engine; // engine to which the object is bound
		uint32_t m_chid; // channel to which the object is bound
		uint32_t m_valid; // whether or not the object is valid
	};

	void logRead(uint32_t addr, uint32_t value);
	void logWrite(uint32_t addr, uint32_t value);
	bool updateIo(bool is_update);
	template<bool is_write, typename T>
	auto getIoFunc(bool log, bool enabled, bool is_be);
	void fifoHandler(std::stop_token stok);
	CoroFrame pusher(const std::stop_token &stok);
	void puller(const std::stop_token &stok, std::coroutine_handle<CoroFrame::promise_type> coro_pusher);
	RamhtElement ramhtSearch(uint32_t handle);

	uint8_t *m_ram;
	std::jthread m_jthr; // async fifo worker thread
	std::atomic_flag m_fifo_has_work;
	std::atomic_flag m_puller_has_err;
	std::atomic_bool m_is_enabled;
	std::mutex m_fifo_mtx;
	// connected devices
	pmc *m_pmc;
	pgraph *m_pgraph;
	pramin *m_pramin;
	cpu_t *m_lc86cpu;
	nv2a *m_nv2a;
	// registers
	uint32_t m_regs[NV_PFIFO_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info =
	{
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
		{ NV_PFIFO_CACHE1_PULL0, "NV_PFIFO_CACHE1_PULL0" },
		{ NV_PFIFO_CACHE1_PULL1, "NV_PFIFO_CACHE1_PULL1" },
		{ NV_PFIFO_CACHE1_HASH, "NV_PFIFO_CACHE1_HASH" },
		{ NV_PFIFO_CACHE1_GET, "NV_PFIFO_CACHE1_GET" },
		{ NV_PFIFO_CACHE1_ENGINE, "NV_PFIFO_CACHE1_ENGINE" },
		{ NV_PFIFO_CACHE1_DMA_DCOUNT, "NV_PFIFO_CACHE1_DMA_DCOUNT" },
		{ NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW, "NV_PFIFO_CACHE1_DMA_GET_JMP_SHADOW" },
		{ NV_PFIFO_CACHE1_DMA_RSVD_SHADOW, "NV_PFIFO_CACHE1_DMA_RSVD_SHADOW" },
		{ NV_PFIFO_CACHE1_DMA_DATA_SHADOW, "NV_PFIFO_CACHE1_DMA_DATA_SHADOW" },
		{ NV_PFIFO_CACHE1_METHOD(0), "NV_PFIFO_CACHE1_METHOD + 0"},
		{ NV_PFIFO_CACHE1_DATA(0), "NV_PFIFO_CACHE1_DATA + 0"}
	};
};

template<bool log, engine_enabled enabled>
void pfifo::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		logWrite(addr, value);
	}

	std::unique_lock lock(m_fifo_mtx);

	switch (addr)
	{
	case NV_PFIFO_INTR_0:
		REG_PFIFO(addr) &= ~value;
		lock.unlock();
		m_pmc->updateIrq();
		break;

	case NV_PFIFO_INTR_EN_0:
		REG_PFIFO(addr) = value;
		lock.unlock();
		m_pmc->updateIrq();
		break;

	case NV_PFIFO_CACHE1_PUSH0: // pusher access to cache1 changed and possibly put != get, notify the pusher
	case NV_PFIFO_CACHE1_DMA_PUSH: // pusher state changed and possibly put != get, notify the pusher
	case NV_PFIFO_CACHE1_DMA_PUT: // dma pointer changed, notify the pusher
	case NV_PFIFO_CACHE1_DMA_GET: // dma pointer changed, notify the pusher
	case NV_PFIFO_CACHE1_PULL0: // puller access to cache1 changed and possibly put != get, notify the pusher
	{
		uint32_t ro_mask = 0;
		if (addr == NV_PFIFO_CACHE1_DMA_PUSH) {
			ro_mask = NV_PFIFO_CACHE1_DMA_PUSH_STATE | NV_PFIFO_CACHE1_DMA_PUSH_BUFFER;
		}
		else if (addr == NV_PFIFO_CACHE1_PULL0) {
			ro_mask = NV_PFIFO_CACHE1_PULL0_HASH | NV_PFIFO_CACHE1_PULL0_DEVICE | NV_PFIFO_CACHE1_PULL0_HASH_STATE;
		}
		REG_PFIFO(addr) = value & ~ro_mask;
		lock.unlock();
		m_fifo_has_work.test_and_set();
		m_fifo_has_work.notify_one();
	}
	break;

	case NV_PFIFO_CACHE1_STATUS:
	case NV_PFIFO_RUNOUT_STATUS:
		// read-only
		break;

	case NV_PFIFO_CACHE1_HASH:
		REG_PFIFO(addr) = value;
		m_puller_has_err.clear();
		lock.unlock();
		m_puller_has_err.notify_one();
		break;

	default:
		REG_PFIFO(addr) = value;
	}
}

template<bool log, engine_enabled enabled>
uint32_t pfifo::Impl::read32(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	m_fifo_mtx.lock();
	uint32_t value = REG_PFIFO(addr);
	m_fifo_mtx.unlock();

	if constexpr (log) {
		logRead(addr, value);
	}

	return value;
}

template<bool log, engine_enabled enabled>
uint8_t pfifo::Impl::read8(uint32_t addr)
{
	if constexpr (!enabled) {
		return 0;
	}

	uint32_t addr_base = addr & ~3;
	uint32_t addr_offset = (addr & 3) << 3;
	uint32_t value32 = read32<false, on>(addr_base);
	uint8_t value = uint8_t((value32 & (0xFF << addr_offset)) >> addr_offset);

	if constexpr (log) {
		logRead(addr_base, value);
	}

	return value;
}

pfifo::Impl::CoroFrame pfifo::Impl::pusher(const std::stop_token &stok)
{
	co_await std::suspend_always(); // switch to caller (this only happens at startup)

	m_fifo_mtx.lock();

	// These two are used when the pusher encounters an error
	std::string err_msg("");
	uint32_t err_code = 0;

	while (true) {
		// Wait until there's some work to do
		m_fifo_mtx.unlock();
		m_fifo_has_work.wait(false);
		m_fifo_mtx.lock();
		m_fifo_has_work.clear();

		if (stok.stop_requested()) [[unlikely]] {
			m_fifo_mtx.unlock();
			throw std::exception();
		}

		if ((((REG_PFIFO(NV_PFIFO_CACHE1_PUSH0) & NV_PFIFO_CACHE1_PUSH0_ACCESS) << 1) |
			(REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) & (NV_PFIFO_CACHE1_DMA_PUSH_ACCESS | NV_PFIFO_CACHE1_DMA_PUSH_STATUS))) ^
			(NV_PFIFO_CACHE1_DMA_PUSH_ACCESS | (NV_PFIFO_CACHE1_PUSH0_ACCESS << 1))) {
			// Pusher is either disabled or suspended, so switch to puller since there might still be entries in cache1
			co_await std::suspend_always();
			continue;
		}

		// We are running, so set the busy flag
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) |= NV_PFIFO_CACHE1_DMA_PUSH_STATE;

		uint32_t curr_pb_get = REG_PFIFO(NV_PFIFO_CACHE1_DMA_GET) & ~3;
		uint32_t curr_pb_put = REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUT) & ~3;
		// Find the address of the new pb entries from the pb object
		DmaObj pb_obj = m_nv2a->getDmaObj((REG_PFIFO(NV_PFIFO_CACHE1_DMA_INSTANCE) & NV_PFIFO_CACHE1_DMA_INSTANCE_ADDRESS) << 4);

		// Process all entries until the fifo is empty
		while (curr_pb_get != curr_pb_put) {
			if (curr_pb_get >= pb_obj.limit) {
				err_msg = "Pusher exception: curr_pb_get >= pb_obj.limit";
				err_code = NV_PFIFO_CACHE1_DMA_STATE_ERROR_PROTECTION; // set mem fault error
				goto pusher_error;
			}
			uint8_t *pb_addr = m_ram + pb_obj.target_addr + curr_pb_get; // ram host base addr + pb base addr + pb offset
			uint32_t pb_entry = *(uint32_t *)pb_addr;
			curr_pb_get += 4;

			uint32_t mthd_cnt = (REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE) & NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT) >> 18; // parameter count of method
			if (mthd_cnt) {
				// A method is already being processed, so the following words must be its parameters

				REG_PFIFO(NV_PFIFO_CACHE1_DMA_DATA_SHADOW) = pb_entry; // save in shadow reg the current entry

				uint32_t cache1_put = REG_PFIFO(NV_PFIFO_CACHE1_PUT) & 0x1FC;
				uint32_t dma_state = REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE);
				uint32_t mthd_type = dma_state & NV_PFIFO_CACHE1_DMA_STATE_METHOD_TYPE; // method type
				uint32_t mthd = dma_state & NV_PFIFO_CACHE1_DMA_STATE_METHOD; // the actual method specified
				uint32_t mthd_subchan = dma_state & NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL; // the bound subchannel

				// Add the method and its parameter to cache1
				REG_PFIFO(NV_PFIFO_CACHE1_METHOD(cache1_put >> 2)) = mthd_type | mthd | mthd_subchan;
				REG_PFIFO(NV_PFIFO_CACHE1_DATA(cache1_put >> 2)) = pb_entry;

				uint32_t cache1_status = REG_PFIFO(NV_PFIFO_CACHE1_STATUS);
				REG_PFIFO(NV_PFIFO_CACHE1_PUT) = (cache1_put + 4) & 0x1FC;
				if (REG_PFIFO(NV_PFIFO_CACHE1_PUT) == REG_PFIFO(NV_PFIFO_CACHE1_GET)) {
					cache1_status |= NV_PFIFO_CACHE1_STATUS_HIGH_MARK; // cache1 full
				}
				if (cache1_status & NV_PFIFO_CACHE1_STATUS_LOW_MARK) {
					cache1_status &= ~NV_PFIFO_CACHE1_STATUS_LOW_MARK; // cache1 no longer empty
				}
				REG_PFIFO(NV_PFIFO_CACHE1_STATUS) = cache1_status;

				// Update dma state
				if (mthd_type == 0) {
					dma_state &= ~NV_PFIFO_CACHE1_DMA_STATE_METHOD;
					dma_state |= (mthd + 4); // increasing method: method increases by one for each parameter
				}
				mthd_cnt--;
				dma_state &= ~NV_PFIFO_CACHE1_DMA_STATE_METHOD_COUNT;
				dma_state |= (mthd_cnt << 18);
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE) = dma_state; // resave dma state with updated method and count
				REG_PFIFO(NV_PFIFO_CACHE1_DMA_DCOUNT)++;

				REG_PFIFO(NV_PFIFO_CACHE1_DMA_GET) = curr_pb_get; // write back updated dma get pointer

				co_await std::suspend_always(); // switch to puller

				if (m_is_enabled == false) {
					// This happens when this engine is disabled while we were in the middle of processing methods
					break;
				}
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
						err_msg = "Pusher exception: call command while another subroutine is already active";
						err_code = NV_PFIFO_CACHE1_DMA_STATE_ERROR_CALL; // set call error
						goto pusher_error;
					}
					REG_PFIFO(NV_PFIFO_CACHE1_DMA_SUBROUTINE) = curr_pb_get | 1;
					curr_pb_get = pb_entry & 0xFFFFFFFC;
				}
				else if (pb_entry == 0x00020000) {
					// return (nv1a+) -> restore pb get addr from subroutine return addr saved with a previous call
					// 00000000000000100000000000000000
					if ((REG_PFIFO(NV_PFIFO_CACHE1_DMA_SUBROUTINE) & 1) == 0) {
						err_msg = "Pusher exception: return command while subroutine is not active";
						err_code = NV_PFIFO_CACHE1_DMA_STATE_ERROR_RETURN; // set return error
						goto pusher_error;
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
					std::array<char, 9> pb_entry_buff{};
					err_msg = "Pusher exception: encountered unrecognized command, pb_entry=0x";
					[[maybe_unused]] const auto &ret = std::to_chars(pb_entry_buff.data(), pb_entry_buff.data() + pb_entry_buff.size(), pb_entry, 16);
					assert(ret.ec == std::errc());
					err_msg += pb_entry_buff.data();
					err_code = NV_PFIFO_CACHE1_DMA_STATE_ERROR_RESERVED_CMD; // set invalid command error
					goto pusher_error;
				}

				REG_PFIFO(NV_PFIFO_CACHE1_DMA_GET) = curr_pb_get; // write back updated dma get pointer
			}
		}

		// We are done with processing, so clear the busy flag
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) &= ~NV_PFIFO_CACHE1_DMA_PUSH_STATE;
		continue;

	pusher_error:
		assert((err_msg != "") && err_code);
		logger_en(warn, err_msg.c_str());
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_STATE) |= (err_code << 29); // set error code
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) &= ~NV_PFIFO_CACHE1_DMA_PUSH_STATE; // no longer busy
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) |= NV_PFIFO_CACHE1_DMA_PUSH_STATUS; // suspend pusher
		REG_PFIFO(NV_PFIFO_INTR_0) |= NV_PFIFO_INTR_0_DMA_PUSHER; // raise pusher interrupt
		m_fifo_mtx.unlock();
		m_pmc->updateIrq();
		err_msg = "";
		err_code = 0;
	}
}

void pfifo::Impl::puller(const std::stop_token &stok, std::coroutine_handle<CoroFrame::promise_type> coro)
{
	coro(); // switch to pusher (this only happens at startup)

	// These three are used when the puller encounters an error
	std::string err_msg("");
	uint32_t err_code = 0;
	auto &&err_handler = [&err_msg, &err_code](RamhtElement elem)
		{
			if (elem.m_valid == NV_RAMHT_STATUS_INVALID) {
				err_msg = "Puller exception: hashing failed, no matching object found. Method 0x%08" PRIX32 ", subchannel %" PRIu32 ", parameter 0x%08" PRIX32;
				err_code = NV_PFIFO_CACHE1_PULL0_HASH;
			}
			else {
				err_msg = "Puller exception: software method. Method 0x%08" PRIX32 ", subchannel %" PRIu32 ", parameter 0x%08" PRIX32;
				err_code = NV_PFIFO_CACHE1_PULL0_DEVICE;
			}
		};

	while (true) {
		uint32_t cache1_status = REG_PFIFO(NV_PFIFO_CACHE1_STATUS);

		if ((REG_PFIFO(NV_PFIFO_CACHE1_PULL0) & NV_PFIFO_CACHE1_PULL0_ACCESS) == 0) {
			// Puller access to cache1 is disabled, switch back to the pusher and push new entries if cache1 is not full
			if (cache1_status & NV_PFIFO_CACHE1_STATUS_HIGH_MARK) {
				// Cache1 is full, so we must wait here
				m_fifo_mtx.unlock();
				m_fifo_has_work.wait(false);
				m_fifo_mtx.lock();
				m_fifo_has_work.clear();

				if (stok.stop_requested()) [[unlikely]] {
					break;
				}
			}
			else {
				coro(); // switch to pusher
			}
			continue;
		}

		if (cache1_status & NV_PFIFO_CACHE1_STATUS_LOW_MARK) {
			// Puller has processed all entries, go back to pusher and wait
			coro();
			continue;
		}

		uint32_t cache1_get = REG_PFIFO(NV_PFIFO_CACHE1_GET);
		REG_PFIFO(NV_PFIFO_CACHE1_GET) = (cache1_get + 4) & 0x1FC;
		if (REG_PFIFO(NV_PFIFO_CACHE1_GET) == REG_PFIFO(NV_PFIFO_CACHE1_PUT)) {
			cache1_status |= NV_PFIFO_CACHE1_STATUS_LOW_MARK; // cache1 empty again
		}
		if (cache1_status & NV_PFIFO_CACHE1_STATUS_HIGH_MARK) {
			cache1_status &= ~NV_PFIFO_CACHE1_STATUS_HIGH_MARK; // cache1 no longer full
		}
		REG_PFIFO(NV_PFIFO_CACHE1_STATUS) = cache1_status;

		uint32_t mthd_entry = REG_PFIFO(NV_PFIFO_CACHE1_METHOD(cache1_get >> 2));
		uint32_t param = REG_PFIFO(NV_PFIFO_CACHE1_DATA(cache1_get >> 2));
		uint32_t mthd = mthd_entry & NV_PFIFO_CACHE1_DMA_STATE_METHOD; // the actual method specified
		uint32_t mthd_subchan = (mthd_entry & NV_PFIFO_CACHE1_DMA_STATE_SUBCHANNEL) >> 13; // the bound subchannel

		if (mthd == 0) {
			// Method zero binds an engine object to a subchannel, and the handle to lookup is the parameter of the method

			RamhtElement elem = ramhtSearch(param);
			if ((elem.m_valid == NV_RAMHT_STATUS_INVALID) || (elem.m_engine == NV_RAMHT_ENGINE_SW)) [[unlikely]] {
				err_handler(elem);
				goto puller_error;
			}
			assert(elem.m_chid == (REG_PFIFO(NV_PFIFO_CACHE1_PUSH1) & NV_PFIFO_CACHE1_PUSH1_CHID)); // should always be the case on xbox
			assert(elem.m_engine == NV_RAMHT_ENGINE_GRAPHICS); // should always be the case on xbox

			// Bind the found engine to subchannel
			(REG_PFIFO(NV_PFIFO_CACHE1_ENGINE) &= ~(3 << (mthd_subchan << 2))) |= (elem.m_engine << (mthd_subchan << 2));
			(REG_PFIFO(NV_PFIFO_CACHE1_PULL1) &= ~NV_PFIFO_CACHE1_PULL1_ENGINE) |= elem.m_engine;

			// Release the lock since submitMethod will block if the pgraph queue is full
			m_fifo_mtx.unlock();
			m_pgraph->submitMethod<true>(0, elem.m_instance, mthd_subchan, elem.m_chid);
			m_fifo_mtx.lock();
		}
		else if (mthd >= 0x100) {
			// Methods >= 0x100 are sent to the currently bound engine

			if ((mthd >= 0x180) && (mthd < 0x200)) {
				// Methods in the range [0x180-0x1fc] require a handle lookup in ramht, which is then sent to the bound engine as parameter

				RamhtElement elem = ramhtSearch(param);
				if ((elem.m_valid == NV_RAMHT_STATUS_INVALID) || (elem.m_engine == NV_RAMHT_ENGINE_SW)) [[unlikely]] {
					err_handler(elem);
					goto puller_error;
				}
				assert(elem.m_chid == (REG_PFIFO(NV_PFIFO_CACHE1_PUSH1) & NV_PFIFO_CACHE1_PUSH1_CHID)); // should always be the case on xbox
				param = elem.m_instance;
			}

			uint32_t bound_engine = (REG_PFIFO(NV_PFIFO_CACHE1_ENGINE) & (3 << (mthd_subchan << 2))) >> (mthd_subchan << 2);
			assert(bound_engine == NV_RAMHT_ENGINE_GRAPHICS); // should always be the case on xbox
			(REG_PFIFO(NV_PFIFO_CACHE1_PULL1) &= ~NV_PFIFO_CACHE1_PULL1_ENGINE) |= bound_engine;

			// Release the lock since submitMethod will block if the pgraph queue is full
			m_fifo_mtx.unlock();
			m_pgraph->submitMethod<false>(mthd, param, mthd_subchan, 0);
			m_fifo_mtx.lock();
		}
		else {
			// TODO: methods executed directly by the puller itself
			nxbx_fatal("Method 0x%08" PRIX32 ", subchannel %" PRIu32 ", parameter 0x%08" PRIX32 " not implemented", mthd, mthd_subchan, param);
			break;
		}

		if (stok.stop_requested()) [[unlikely]] {
			break;
		}

		continue;

	puller_error:
		assert((err_msg != "") && err_code);
		logger_en(warn, err_msg.c_str(), mthd, mthd_subchan, param);
		REG_PFIFO(NV_PFIFO_CACHE1_PULL0) |= err_code; // set error code
		REG_PFIFO(NV_PFIFO_CACHE1_DMA_PUSH) &= ~NV_PFIFO_CACHE1_DMA_PUSH_STATE; // clear pusher busy flag
		REG_PFIFO(NV_PFIFO_CACHE1_GET) = cache1_get; // restart from the faulting method
		REG_PFIFO(NV_PFIFO_INTR_0) |= NV_PFIFO_INTR_0_CACHE_ERROR; // raise puller interrupt
		m_puller_has_err.test_and_set();
		m_fifo_mtx.unlock();
		m_pmc->updateIrq();
		err_msg = "";
		err_code = 0;
		m_puller_has_err.wait(true);
		m_fifo_mtx.lock();
		if (stok.stop_requested()) [[unlikely]] {
			break;
		}
	}

	m_fifo_mtx.unlock();
	while (true) {
		if (stok.stop_requested()) { // sync with pfifo::Impl::deinit
			throw std::exception();
		}
	}
}

void pfifo::Impl::fifoHandler(std::stop_token stok)
{
	// This function is called in a separate thread, and acts as the pfifo pusher and puller

	std::coroutine_handle<CoroFrame::promise_type> coro;

	try {
		coro = pusher(stok).m_handle; // grab coro handle
		puller(stok, coro);
	}
	catch (std::exception e) {
		// Just fallthrough
	}

	coro.destroy();
}

pfifo::Impl::RamhtElement pfifo::Impl::ramhtSearch(uint32_t handle)
{
	// An object is referenced by a user defined 32 bit handle. The hw looks up objects in a hash table in the instance memory (ramin)
	// An entry in the table consists of two DWORDs. The first is a handle, and the second is a context the describes the object
	// The context DWORD is as follows:
	// 15: 0  instance_addr >> 4
	// 17:16  engine (0=sw,1=graphics,2=dvd)
	// 28:24  channel id
	// 31	  valid (1=ok,0=bad)

	uint32_t ramht_size = 1 << (((REG_PFIFO(NV_PFIFO_RAMHT) & NV_PFIFO_RAMHT_SIZE) >> 16) + 12);
	uint32_t curr_chan_id = REG_PFIFO(NV_PFIFO_CACHE1_PUSH1) & NV_PFIFO_CACHE1_PUSH1_CHID;
	uint32_t ramht_bits = std::countr_zero(ramht_size) - 1;
	uint32_t hash = 0;

	// We are hashing, so set the busy flag
	REG_PFIFO(NV_PFIFO_CACHE1_PULL0) |= NV_PFIFO_CACHE1_PULL0_HASH_STATE;

	// Same algorithm as used in nouveau
	while (handle) {
		hash ^= (handle & ((1 << ramht_bits) - 1));
		handle >>= ramht_bits;
	}
	hash ^= curr_chan_id << (ramht_bits - 4);

	// We are done with hashing, so clear the busy flag
	REG_PFIFO(NV_PFIFO_CACHE1_PULL0) &= ~NV_PFIFO_CACHE1_PULL0_HASH_STATE;

	uint32_t ramht_addr = (REG_PFIFO(NV_PFIFO_RAMHT) & NV_PFIFO_RAMHT_BASE_ADDRESS) << 8;
	uint32_t entry_handle = m_pramin->read32(NV_PRAMIN_BASE + ramht_addr + hash * 8);
	uint32_t entry_ctx = m_pramin->read32(NV_PRAMIN_BASE + ramht_addr + 4 + hash * 8);

	return RamhtElement{
		.m_handle = entry_handle,
		.m_instance = (entry_ctx & NV_RAMHT_INSTANCE) << 4,
		.m_engine = (entry_ctx & NV_RAMHT_ENGINE) >> 16,
		.m_chid = (entry_ctx & NV_RAMHT_CHID) >> 24,
		.m_valid = (entry_ctx & NV_RAMHT_STATUS) >> 31,
	};
}

void
pfifo::Impl::logRead(uint32_t addr, uint32_t value)
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
			logger<log_lv::debug, log_module::pfifo, false>("Read at UNKNOWN (0x%08X) of value 0x%08X", addr, value);
		}
	}
}

void
pfifo::Impl::logWrite(uint32_t addr, uint32_t value)
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
			logger<log_lv::debug, log_module::pfifo, false>("Write at UNKNOWN (0x%08X) of value 0x%08X", addr, value);
		}
	}
}

template<bool is_write, typename T>
auto pfifo::Impl::getIoFunc(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pfifo::Impl, uint32_t, &pfifo::Impl::write32<true, on>, big> : nv2a_write<pfifo::Impl, uint32_t, &pfifo::Impl::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pfifo::Impl, uint32_t, &pfifo::Impl::write32<false, on>, big> : nv2a_write<pfifo::Impl, uint32_t, &pfifo::Impl::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pfifo::Impl, uint32_t, &pfifo::Impl::write32<false, off>, big>;
		}
	}
	else {
		if constexpr (sizeof(T) == 1) {
			if (enabled) {
				if (log) {
					return is_be ? nv2a_read<pfifo::Impl, uint8_t, &pfifo::Impl::read8<true, on>, big> : nv2a_read<pfifo::Impl, uint8_t, &pfifo::Impl::read8<true, on>, le>;
				}
				else {
					return is_be ? nv2a_read<pfifo::Impl, uint8_t, &pfifo::Impl::read8<false, on>, big> : nv2a_read<pfifo::Impl, uint8_t, &pfifo::Impl::read8<false, on>, le>;
				}
			}
			else {
				return nv2a_read<pfifo::Impl, uint8_t, &pfifo::Impl::read8<false, off>, big>;
			}
		}
		else {
			if (enabled) {
				if (log) {
					return is_be ? nv2a_read<pfifo::Impl, uint32_t, &pfifo::Impl::read32<true, on>, big> : nv2a_read<pfifo::Impl, uint32_t, &pfifo::Impl::read32<true, on>, le>;
				}
				else {
					return is_be ? nv2a_read<pfifo::Impl, uint32_t, &pfifo::Impl::read32<false, on>, big> : nv2a_read<pfifo::Impl, uint32_t, &pfifo::Impl::read32<false, on>, le>;
				}
			}
			else {
				return nv2a_read<pfifo::Impl, uint32_t, &pfifo::Impl::read32<false, off>, big>;
			}
		}
	}
}

bool
pfifo::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	m_is_enabled = m_pmc->read32(NV_PMC_ENABLE) & NV_PMC_ENABLE_PFIFO;
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PFIFO_BASE, NV_PFIFO_SIZE, false,
		{
			.fnr8 = getIoFunc<false, uint8_t>(log, m_is_enabled, is_be),
			.fnr32 = getIoFunc<false, uint32_t>(log, m_is_enabled, is_be),
			.fnw32 = getIoFunc<true, uint32_t>(log, m_is_enabled, is_be)
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pfifo::Impl::reset()
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
pfifo::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_pgraph = gpu->getPgraph();
	m_pramin = gpu->getPramin();
	m_lc86cpu = cpu->get86cpu();
	m_nv2a = gpu;
	reset();

	if (!updateIo(false)) {
		return false;
	}

	m_ram = get_ram_ptr(m_lc86cpu);
	m_jthr = std::jthread(std::bind_front(&pfifo::Impl::fifoHandler, this));
	return true;
}

void pfifo::Impl::deinit()
{
	assert(m_jthr.joinable());
	m_jthr.request_stop();
	if (m_pgraph) {
		m_pgraph->deinit();
	}
	m_puller_has_err.clear();
	m_puller_has_err.notify_one();
	m_fifo_has_work.test_and_set();
	m_fifo_has_work.notify_one();
	m_jthr.join();
}

/** Public interface implementation **/
bool pfifo::init(cpu *cpu, nv2a *gpu)
{
	return m_impl->init(cpu, gpu);
}

void pfifo::deinit()
{
	m_impl->deinit();
}

void pfifo::reset()
{
	m_impl->reset();
}

void pfifo::updateIo()
{
	m_impl->updateIo();
}

uint32_t pfifo::read32(uint32_t addr)
{
	return m_impl->read32<false, on>(addr);
}

void pfifo::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false, on>(addr, value);
}

pfifo::pfifo() : m_impl{std::make_unique<pfifo::Impl>()} {}
pfifo::~pfifo() {}
