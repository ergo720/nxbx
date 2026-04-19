// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2025 ergo720

#include "lib86cpu.hpp"
#include "pramin.hpp"
#include "pmc.hpp"
#include "pgraph.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include "util.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <cassert>
#include <cinttypes>
#include <cstring>
#ifdef _WIN32
#undef max
#endif
#include "spsc-queue.hpp"

#define MODULE_NAME pgraph

#define SET_REG(reg, mask, val) (REG_PGRAPH(reg) &= ~(mask)) |= (val)
#define REG_PGRAPH_ptr(r) (impl->m_regs[REGS_PGRAPH_idx(r)])
#define IMPL(class_) auto class_impl = &impl->class_
#define MTHD_HANDLER_ARGS pgraph::ImplAlias *impl, uint32_t mthd, uint32_t param, uint32_t subchan
#define UNBOUND_OBJ_ADDR -1U

// Macros used in InputQueueEntry for ctx switches
#define CTX_SWITCH_CHID 0x1F // target channel
#define CTX_SWITCH_STATUS (1 << 31) // switch requested=1

#include "nv2a_classes.hpp"

struct NvNotification
{
	uint64_t timestamp; // ns elapsed since January 1st, 1970
	uint32_t info32; // the active object when the error occurred
	uint16_t info16; // the method invoked when the error occurred
	uint16_t status; // the completion status of the method that notified
};
static_assert(sizeof(NvNotification) == 16, "sizeof(NvNotification) == 16");


/** Private device implementation **/
class pgraph::Impl
{
public:
	void init(cpu *cpu, nv2a *gpu);
	void deinit();
	void reset();
	void updateIo() { updateIo(true); }
	template<bool log, engine_enabled enabled>
	uint32_t read32(uint32_t addr);
	template<bool log, engine_enabled enabled>
	void write32(uint32_t addr, const uint32_t value);
	template<bool is_mthd_zero>
	void submitMethod(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t chid)
	{
		// called from the fifo thread
		uint32_t ctx_switch = 0;
		if constexpr (is_mthd_zero) {
			ctx_switch = chid | CTX_SWITCH_STATUS;
		}
		submitMethod(mthd, param, subchan, ctx_switch);
	}

	// method friend declarations
	friend void unimplemented_method(MTHD_HANDLER_ARGS);

	friend void dispatch_nv039(MTHD_HANDLER_ARGS);
	friend void NV039_SET_OBJECT(MTHD_HANDLER_ARGS);
	friend void NV039_SET_CONTEXT_DMA_NOTIFIES(MTHD_HANDLER_ARGS);

	friend void dispatch_nv062(MTHD_HANDLER_ARGS);
	friend void NV062_SET_OBJECT(MTHD_HANDLER_ARGS);
	friend void NV062_SET_CONTEXT_DMA_IMAGE_SOURCE(MTHD_HANDLER_ARGS);
	friend void NV062_SET_CONTEXT_DMA_IMAGE_DESTIN(MTHD_HANDLER_ARGS);

	friend void dispatch_nv097(MTHD_HANDLER_ARGS);
	friend void NV097_SET_OBJECT(MTHD_HANDLER_ARGS);

	friend void dispatch_nv09f(MTHD_HANDLER_ARGS);
	friend void NV09F_SET_OBJECT(MTHD_HANDLER_ARGS);
	friend void NV09F_SET_CONTEXT_COLOR_KEY(MTHD_HANDLER_ARGS);
	friend void NV09F_SET_OPERATION(MTHD_HANDLER_ARGS);

private:
	struct InputQueueEntry
	{
		uint32_t m_mthd;
		uint32_t m_param;
		uint32_t m_subchan;
		uint32_t m_ctx_switch;
	};

	void logRead(uint32_t addr, uint32_t value);
	void logWrite(uint32_t addr, uint32_t value);
	void updateIo(bool is_update);
	template<bool is_write>
	auto getIoFunc(bool log, bool enabled, bool is_be);
	void graphHandler(std::stop_token stok);
	void submitMethod(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t ctx_switch);

	uint8_t *m_ram;
	std::jthread m_jthr; // async graphics worker thread
	std::atomic_flag m_graph_has_work;
	std::atomic_flag m_ctx_switch_trig;
	std::atomic_uint32_t m_is_enabled;
	std::mutex m_graph_mtx;
	dro::SPSCQueue<InputQueueEntry> m_input_queue{256};
	uint32_t m_should_exit; // 0: exit, 3: continue
	// classes states
	struct
	{
		// NV03_MEMORY_TO_MEMORY_FORMAT
		uint32_t m_instance_addr;
		uint32_t m_notification_addr;
		bool m_notification_active[2];
	} m_memcpy;
	struct
	{
		// NV10_CONTEXT_SURFACES_2D
		uint32_t m_instance_addr;
		uint32_t m_img_src_addr;
		uint32_t m_img_dst_addr;
	} m_ctx_surfaces_2d;
	struct
	{
		// NV20_KELVIN_PRIMITIVE
		uint32_t m_instance_addr;
	} m_kelvin;
	struct
	{
		// NV15_IMAGE_BLIT
		uint32_t m_instance_addr;
		uint32_t m_color_key_instance_addr;
		uint32_t m_operation;
	} m_img_blit;
	// Make sure we can safely use memset on the method classes structs
	static_assert(std::is_trivially_copyable_v<decltype(m_memcpy)>);
	static_assert(std::is_trivially_copyable_v<decltype(m_ctx_surfaces_2d)>);
	static_assert(std::is_trivially_copyable_v<decltype(m_kelvin)>);
	static_assert(std::is_trivially_copyable_v<decltype(m_img_blit)>);
	// connected devices
	pmc *m_pmc;
	pramin *m_pramin;
	cpu_t *m_lc86cpu;
	nv2a *m_nv2a;
	// atomic registers
	std::atomic_uint32_t m_int_status;
	std::atomic_uint32_t m_int_enabled;
	std::atomic_uint32_t m_fifo_access;
	std::atomic_uint32_t m_busy;
	// registers
	uint32_t m_regs[NV_PGRAPH_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info =
	{
		{ NV_PGRAPH_DEBUG_3, "NV_PGRAPH_DEBUG_3" },
		{ NV_PGRAPH_INTR, "NV_PGRAPH_INTR" },
		{ NV_PGRAPH_INTR_EN, "NV_PGRAPH_INTR_EN" },
		{ NV_PGRAPH_CTX_CONTROL, "NV_PGRAPH_CTX_CONTROL" },
		{ NV_PGRAPH_CTX_USER, "NV_PGRAPH_CTX_USER" },
		{ NV_PGRAPH_CTX_SWITCH1, "NV_PGRAPH_CTX_SWITCH1"},
		{ NV_PGRAPH_CTX_SWITCH2, "NV_PGRAPH_CTX_SWITCH2"},
		{ NV_PGRAPH_CTX_SWITCH3, "NV_PGRAPH_CTX_SWITCH3"},
		{ NV_PGRAPH_CTX_SWITCH4, "NV_PGRAPH_CTX_SWITCH4"},
		{ NV_PGRAPH_CTX_SWITCH5, "NV_PGRAPH_CTX_SWITCH5"},
		{ NV_PGRAPH_CTX_CACHE1(0), "NV_PGRAPH_CTX_CACHE"},
		{ NV_PGRAPH_CTX_CACHE2(0), "NV_PGRAPH_CTX_CACHE"},
		{ NV_PGRAPH_CTX_CACHE3(0), "NV_PGRAPH_CTX_CACHE"},
		{ NV_PGRAPH_CTX_CACHE4(0), "NV_PGRAPH_CTX_CACHE"},
		{ NV_PGRAPH_CTX_CACHE5(0), "NV_PGRAPH_CTX_CACHE"},
		{ NV_PGRAPH_STATUS, "NV_PGRAPH_STATUS" },
		{ NV_PGRAPH_TRAPPED_ADDR, "NV_PGRAPH_TRAPPED_ADDR"},
		{ NV_PGRAPH_FIFO, "NV_PGRAPH_FIFO" },
		{ NV_PGRAPH_CHANNEL_CTX_POINTER, "NV_PGRAPH_CHANNEL_CTX_POINTER" },
		{ NV_PGRAPH_CHANNEL_CTX_TRIGGER, "NV_PGRAPH_CHANNEL_CTX_TRIGGER" },
	};
};

/** Method implementations **/
void unimplemented_method(MTHD_HANDLER_ARGS)
{
	uint32_t gr_class = REG_PGRAPH_ptr(NV_PGRAPH_CTX_SWITCH1) & NV_PGRAPH_CTX_SWITCH1_GRCLASS;
	nxbx_fatal("%s: class 0x%08" PRIX32 ", method 0x%08" PRIX32 ", subchannel %" PRIu32 ", parameter 0x%08" PRIX32, __func__, gr_class, mthd, subchan, param);
	impl->m_should_exit = 0;
	impl->m_graph_has_work.test_and_set();
}

void NV039_SET_OBJECT(MTHD_HANDLER_ARGS)
{
	// Binds the engine object to the subchannel
	impl->m_memcpy.m_instance_addr = param;
}

void NV039_SET_CONTEXT_DMA_NOTIFIES(MTHD_HANDLER_ARGS)
{
	// param is the instance address of an array of two NvNotification structs. The first element is used by NV039_NOTIFY, and the second is used by NV039_BUFFER_NOTIFY.
	// Both notifications need to be first activated with NV039_NOTIFY and NV039_BUFFER_NOTIFY respectively. Once activated, the first notification is populated by the
	// gpu following the completion of the next method that is not a notification method itself

	IMPL(m_memcpy);
	class_impl->m_notification_addr = param;
	class_impl->m_notification_active[NV039_NOTIFIERS_NOTIFY] = false;
}

void NV062_SET_OBJECT(MTHD_HANDLER_ARGS)
{
	// Binds the engine object to the subchannel
	impl->m_ctx_surfaces_2d.m_instance_addr = param;
}

void NV062_SET_CONTEXT_DMA_IMAGE_SOURCE(MTHD_HANDLER_ARGS)
{
	// Sets the address of the dma object that translates the address of the src image used during a blit
	impl->m_ctx_surfaces_2d.m_img_src_addr = param;
}

void NV062_SET_CONTEXT_DMA_IMAGE_DESTIN(MTHD_HANDLER_ARGS)
{
	// Sets the address of the dma object that translates the address of the dst image used during a blit
	impl->m_ctx_surfaces_2d.m_img_dst_addr = param;
}

void NV097_SET_OBJECT(MTHD_HANDLER_ARGS)
{
	// Binds the engine object to the subchannel
	impl->m_kelvin.m_instance_addr = param;
}

void NV09F_SET_OBJECT(MTHD_HANDLER_ARGS)
{
	// Binds the engine object to the subchannel
	impl->m_img_blit.m_instance_addr = param;
}

void NV09F_SET_CONTEXT_COLOR_KEY(MTHD_HANDLER_ARGS)
{
	// (Un)binds an instance of NV04_CONTEXT_COLOR_KEY to the subchannel. Basically, enables/disables color keying used in a blit
	DmaObj obj = impl->m_nv2a->getDmaObj(param);
	assert((obj.class_type == NV04_CONTEXT_COLOR_KEY) || (obj.class_type == NV01_NULL));
	impl->m_img_blit.m_color_key_instance_addr = obj.class_type == NV04_CONTEXT_COLOR_KEY ? param : UNBOUND_OBJ_ADDR;
}

void NV09F_SET_OPERATION(MTHD_HANDLER_ARGS)
{
	// Sets the operation to use for the blitting between the src and dst images
	impl->m_img_blit.m_operation = param;
}

/** Method table declarations **/
// NOTE: msvc has a hard limit of 128 nesting levels while compiling code, which will be reached if putting all method cases
// in a single function. To avoid that, we split the if/else statements in multiple functions after 100 methods

#define MTHD_BEGIN(func) if (mthd == (std::to_underlying(EnumT::func) / 4)) {\
return &func;\
}
#define MTHD_CASE(func) else if (mthd == (std::to_underlying(EnumT::func) / 4)) {\
return &func;\
}
#define MTHD_RANGE(func, start, end) else if ((mthd >= (std::to_underlying(EnumT::func ## start) / 4)) && (mthd <= (std::to_underlying(EnumT::func ## end) / 4))) {\
return &func;\
}
#define MTHD_END() \
return unimplemented_method

#define MTHD_CALL(func) \
return func(mthd)

using mthd_func = void(*)(pgraph::ImplAlias *, uint32_t, uint32_t, uint32_t);

template<typename EnumT>
constexpr auto dispatch_func_nv039(uint32_t mthd)
{
	MTHD_BEGIN(NV039_SET_OBJECT)
		MTHD_CASE(NV039_SET_CONTEXT_DMA_NOTIFIES)
	MTHD_END();
}

template<typename EnumT>
constexpr auto dispatch_func_nv062(uint32_t mthd)
{
	MTHD_BEGIN(NV062_SET_OBJECT)
		MTHD_CASE(NV062_SET_CONTEXT_DMA_IMAGE_SOURCE)
		MTHD_CASE(NV062_SET_CONTEXT_DMA_IMAGE_DESTIN)
	MTHD_END();
}

template<typename EnumT>
constexpr auto dispatch_func_nv097(uint32_t mthd)
{
	MTHD_BEGIN(NV097_SET_OBJECT)
	MTHD_END();
}

template<typename EnumT>
constexpr auto dispatch_func_nv09f(uint32_t mthd)
{
	MTHD_BEGIN(NV09F_SET_OBJECT)
		MTHD_CASE(NV09F_SET_CONTEXT_COLOR_KEY)
		MTHD_CASE(NV09F_SET_OPERATION)
	MTHD_END();
}

#undef MTHD_BEGIN
#undef MTHD_CASE
#undef MTHD_RANGE
#undef MTHD_END
#undef MTHD_CALL

template<typename EnumT>
constexpr auto dispatch_func(uint32_t mthd)
{
	if constexpr (std::is_same_v<EnumT, nv039>) {
		return dispatch_func_nv039<EnumT>(mthd);
	}
	else if constexpr (std::is_same_v<EnumT, nv062>) {
		return dispatch_func_nv062<EnumT>(mthd);
	}
	else if constexpr (std::is_same_v<EnumT, nv097>) {
		return dispatch_func_nv097<EnumT>(mthd);
	}
	else if constexpr (std::is_same_v<EnumT, nv09f>) {
		return dispatch_func_nv09f<EnumT>(mthd);
	}
	else {
		static_assert(false, "Unknown class type specified");
	}
}

template<std::size_t Length, typename Generator, std::size_t... Indexes>
constexpr auto gen_mthd_table_impl(Generator&& f, std::index_sequence<Indexes...>)
{
	return std::array<mthd_func, Length> {{ f(Indexes)... }};
}

template<std::size_t Length, typename Generator>
constexpr auto gen_mthd_table(Generator&& f)
{
	return gen_mthd_table_impl<Length>(std::forward<Generator>(f), std::make_index_sequence<Length>{});
}

// The size of 2048 (instead of the expected 8192) relies on the fact that all method numbers are multiples of four. This is because they represent
// the offset they have when accessed from a channel in PIO mode, that is, the offset from the start of the corresponding class MMIO area
static constexpr std::array<mthd_func, 2048> s_method_table_nv039 = gen_mthd_table<2048>(dispatch_func<nv039>);
static constexpr std::array<mthd_func, 2048> s_method_table_nv062 = gen_mthd_table<2048>(dispatch_func<nv062>);
static constexpr std::array<mthd_func, 2048> s_method_table_nv097 = gen_mthd_table<2048>(dispatch_func<nv097>);
static constexpr std::array<mthd_func, 2048> s_method_table_nv09f = gen_mthd_table<2048>(dispatch_func<nv09f>);

void dispatch_nv039(MTHD_HANDLER_ARGS)
{
	uint32_t idx = mthd >> 2;
	ASSUME(idx < s_method_table_nv039.size());
	mthd_func func = s_method_table_nv039[idx];
	ASSUME(func);
	func(impl, mthd, param, subchan);
}

void dispatch_nv062(MTHD_HANDLER_ARGS)
{
	uint32_t idx = mthd >> 2;
	ASSUME(idx < s_method_table_nv062.size());
	mthd_func func = s_method_table_nv062[idx];
	ASSUME(func);
	func(impl, mthd, param, subchan);
}

void dispatch_nv097(MTHD_HANDLER_ARGS)
{
	uint32_t idx = mthd >> 2;
	ASSUME(idx < s_method_table_nv097.size());
	mthd_func func = s_method_table_nv097[idx];
	ASSUME(func);
	func(impl, mthd, param, subchan);
}

void dispatch_nv09f(MTHD_HANDLER_ARGS)
{
	uint32_t idx = mthd >> 2;
	ASSUME(idx < s_method_table_nv09f.size());
	mthd_func func = s_method_table_nv09f[idx];
	ASSUME(func);
	func(impl, mthd, param, subchan);
}

static constexpr std::array<mthd_func, HIGHEST_CLASS + 1> s_method_table_classes = []()
	{
		std::array<std::pair<mthd_func, unsigned>, 4> class_arr
		{ {
			{ &dispatch_nv039, NV03_MEMORY_TO_MEMORY_FORMAT },
			{ &dispatch_nv062, NV10_CONTEXT_SURFACES_2D },
			{ &dispatch_nv097, NV20_KELVIN_PRIMITIVE },
			{ &dispatch_nv09f, NV15_IMAGE_BLIT },
		} };
		std::array<mthd_func, HIGHEST_CLASS + 1> local_arr;
		std::fill(local_arr.begin(), local_arr.end(), &unimplemented_method);
		std::for_each(class_arr.begin(), class_arr.end(), [&local_arr](auto pair)
			{
				local_arr[pair.second] = pair.first;
			});
		return local_arr;
	}();

template<bool log, engine_enabled enabled>
void pgraph::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (!enabled) {
		return;
	}
	if constexpr (log) {
		logWrite(addr, value);
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
		m_pmc->updateIrq();
		break;

	case NV_PGRAPH_INTR_EN:
		m_int_enabled = value;
		m_pmc->updateIrq();
		break;

	case NV_PGRAPH_STATUS:
	case NV_PGRAPH_TRAPPED_ADDR:
		// read-only
		break;

	case NV_PGRAPH_FIFO:
		m_fifo_access = value;
		m_graph_has_work.test_and_set();
		m_graph_has_work.notify_one();
		break;

	case NV_PGRAPH_CHANNEL_CTX_TRIGGER:
		if (value & NV_PGRAPH_CHANNEL_CTX_TRIGGER_READ_IN) {
			m_graph_mtx.lock();
			uint32_t gr_ctx_addr = (REG_PGRAPH(NV_PGRAPH_CHANNEL_CTX_POINTER) & NV_PGRAPH_CHANNEL_CTX_POINTER_INST) << 4;
			uint32_t gr_ctx = m_pramin->read32(gr_ctx_addr);
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
uint32_t pgraph::Impl::read32(uint32_t addr)
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

	case NV_PGRAPH_STATUS:
		value = m_busy;
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
		logRead(addr, value);
	}

	return value;
}

void pgraph::Impl::submitMethod(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t ctx_switch)
{
	// called from the fifo thread
	m_input_queue.emplace(mthd, param, subchan, ctx_switch); // blocks if the queue is full
	m_graph_has_work.test_and_set();
	m_graph_has_work.notify_one();
}

void pgraph::Impl::graphHandler(std::stop_token stok)
{
	m_should_exit = 3;

	while (true) {
		// Wait until the puller pushes some methods
		m_graph_has_work.wait(false);
		std::unique_lock lock(m_graph_mtx);
		m_graph_has_work.clear();

		if (stok.stop_requested()) [[unlikely]] {
			break;
		}

		// We are going to process methods, set the busy flag
		m_busy |= NV_PGRAPH_STATUS_STATE;

		while (!m_input_queue.empty()) {
			if (uint32_t access_granted = (m_fifo_access & NV_PGRAPH_FIFO_ACCESS) // fifo access to graph is disabled, keep looping
				| m_is_enabled // need to check this too because fifo will keep submitting methods even when this engine is disabled
				& m_should_exit; // this thread encountered a fatal error
				access_granted != 3) {
				break;
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
						m_should_exit = 0;
						break;
					}
					m_busy &= ~NV_PGRAPH_STATUS_STATE; // clear the busy flag
					SET_REG(NV_PGRAPH_TRAPPED_ADDR, NV_PGRAPH_TRAPPED_ADDR_CHID, target_chid << 20); // write channel exception data
					m_int_status |= NV_PGRAPH_INTR_CONTEXT_SWITCH; // raise graph interrupt
					m_ctx_switch_trig.test_and_set();
					lock.unlock();
					m_pmc->updateIrq();
					m_ctx_switch_trig.wait(true); // wait until the title does the switch and clears the interrupt
					lock.lock();

					if (stok.stop_requested()) [[unlikely]] {
						break;
					}
					m_busy |= NV_PGRAPH_STATUS_STATE; // reset the busy flag
				}

				// Cache the object context to the appropriate subchannel
				uint32_t ctx_addr = elem.m_param;
				uint32_t ctx1 = m_pramin->read32(ctx_addr);
				uint32_t ctx2 = m_pramin->read32(ctx_addr + 4);
				uint32_t ctx3 = m_pramin->read32(ctx_addr + 8);
				uint32_t ctx4 = m_pramin->read32(ctx_addr + 12);
				uint32_t ctx5 = ctx_addr;
				REG_PGRAPH(NV_PGRAPH_CTX_CACHE1(elem.m_subchan)) = ctx1;
				REG_PGRAPH(NV_PGRAPH_CTX_CACHE2(elem.m_subchan)) = ctx2;
				REG_PGRAPH(NV_PGRAPH_CTX_CACHE3(elem.m_subchan)) = ctx3;
				REG_PGRAPH(NV_PGRAPH_CTX_CACHE4(elem.m_subchan)) = ctx4;
				REG_PGRAPH(NV_PGRAPH_CTX_CACHE5(elem.m_subchan)) = ctx5;
			}

			// Switch to the object context for the appropriate subchannel from the cache
			REG_PGRAPH(NV_PGRAPH_CTX_SWITCH1) = REG_PGRAPH(NV_PGRAPH_CTX_CACHE1(elem.m_subchan));
			REG_PGRAPH(NV_PGRAPH_CTX_SWITCH2) = REG_PGRAPH(NV_PGRAPH_CTX_CACHE2(elem.m_subchan));
			REG_PGRAPH(NV_PGRAPH_CTX_SWITCH3) = REG_PGRAPH(NV_PGRAPH_CTX_CACHE3(elem.m_subchan));
			REG_PGRAPH(NV_PGRAPH_CTX_SWITCH4) = REG_PGRAPH(NV_PGRAPH_CTX_CACHE4(elem.m_subchan));
			REG_PGRAPH(NV_PGRAPH_CTX_SWITCH5) = REG_PGRAPH(NV_PGRAPH_CTX_CACHE5(elem.m_subchan));

			uint32_t gr_class = REG_PGRAPH(NV_PGRAPH_CTX_SWITCH1) & NV_PGRAPH_CTX_SWITCH1_GRCLASS;
			logger_en(debug, "Class 0x%08" PRIX32 ", method 0x%08" PRIX32 ", subchannel %" PRIu32 ", parameter 0x%08" PRIX32, gr_class, elem.m_mthd, elem.m_subchan, elem.m_param);

			ASSUME(gr_class < s_method_table_classes.size());
			mthd_func func = s_method_table_classes[gr_class];
			ASSUME(func);
			func(this, elem.m_mthd, elem.m_param, elem.m_subchan);
		}

		// Done with processing methods, clear the busy flag
		m_busy &= ~NV_PGRAPH_STATUS_STATE;
	}

	// NOTE: it's safe to drain the queue only from the consumer thread
	while (true) {
		while (!m_input_queue.empty()) {
			InputQueueEntry elem;
			m_input_queue.pop(elem);
		}
		if (stok.stop_requested()) { // sync with pgraph::Impl::deinit
			return;
		}
	}
}

void
pgraph::Impl::logRead(uint32_t addr, uint32_t value)
{
	const auto it = m_regs_info.find(addr & ~3);
	if (it != m_regs_info.end()) {
		if (util::in_range(addr, NV_PGRAPH_CTX_CACHE1(0), NV_PGRAPH_CTX_CACHE5(7) + 3)) {
			uint32_t num = ((addr >> 5) & 7) - 2;
			uint32_t subchannel = (addr & 0x1F) >> 2;
			logger<log_lv::debug, log_module::pgraph, false>("Read at %s%u (0x%08X) of value 0x%08X (subchannel %u)", it->second, num, addr, value, subchannel);
		}
		else {
			logger<log_lv::debug, log_module::pgraph, false>("Read at %s (0x%08X) of value 0x%08X", it->second.c_str(), addr, value);
		}
	}
	else {
		logger<log_lv::debug, log_module::pgraph, false>("Read at UNKNOWN (0x%08X) of value 0x%08X", addr, value);
	}
}

void
pgraph::Impl::logWrite(uint32_t addr, uint32_t value)
{
	const auto it = m_regs_info.find(addr & ~3);
	if (it != m_regs_info.end()) {
		if (util::in_range(addr, NV_PGRAPH_CTX_CACHE1(0), NV_PGRAPH_CTX_CACHE5(7) + 3)) {
			uint32_t num = ((addr >> 5) & 7) - 2;
			uint32_t subchannel = (addr & 0x1F) >> 2;
			logger<log_lv::debug, log_module::pgraph, false>("Write at %s%u (0x%08X) of value 0x%08X (subchannel %u)", it->second, num, addr, value, subchannel);
		}
		else {
			logger<log_lv::debug, log_module::pgraph, false>("Write at %s (0x%08X) of value 0x%08X", it->second.c_str(), addr, value);
		}
	}
	else {
		logger<log_lv::debug, log_module::pgraph, false>("Write at UNKNOWN (0x%08X) of value 0x%08X", addr, value);
	}
}

template<bool is_write>
auto pgraph::Impl::getIoFunc(bool log, bool enabled, bool is_be)
{
	if constexpr (is_write) {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_write<pgraph::Impl, uint32_t, &pgraph::Impl::write32<true, on>, big> : nv2a_write<pgraph::Impl, uint32_t, &pgraph::Impl::write32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_write<pgraph::Impl, uint32_t, &pgraph::Impl::write32<false, on>, big> : nv2a_write<pgraph::Impl, uint32_t, &pgraph::Impl::write32<false, on>, le>;
			}
		}
		else {
			return nv2a_write<pgraph::Impl, uint32_t, &pgraph::Impl::write32<false, off>, big>;
		}
	}
	else {
		if (enabled) {
			if (log) {
				return is_be ? nv2a_read<pgraph::Impl, uint32_t, &pgraph::Impl::read32<true, on>, big> : nv2a_read<pgraph::Impl, uint32_t, &pgraph::Impl::read32<true, on>, le>;
			}
			else {
				return is_be ? nv2a_read<pgraph::Impl, uint32_t, &pgraph::Impl::read32<false, on>, big> : nv2a_read<pgraph::Impl, uint32_t, &pgraph::Impl::read32<false, on>, le>;
			}
		}
		else {
			return nv2a_read<pgraph::Impl, uint32_t, &pgraph::Impl::read32<false, off>, big>;
		}
	}
}

void pgraph::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	m_is_enabled = (m_pmc->read32(NV_PMC_ENABLE) & NV_PMC_ENABLE_PGRAPH) >> 11;
	bool is_be = m_pmc->read32(NV_PMC_BOOT_1) & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PGRAPH_BASE, NV_PGRAPH_SIZE, false,
		{
			.fnr32 = getIoFunc<false>(log, m_is_enabled, is_be),
			.fnw32 = getIoFunc<true>(log, m_is_enabled, is_be)
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update mmio region"));
	}
	m_graph_has_work.test_and_set();
	m_graph_has_work.notify_one();
}

void pgraph::Impl::reset()
{
	m_int_status = 0;
	m_int_enabled = 0;
	m_fifo_access = 0;
	m_busy = 0;
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_graph_has_work.clear();

	// Also reset all classes states
	std::memset(&m_memcpy, 0, sizeof(m_memcpy));
	std::memset(&m_ctx_surfaces_2d, 0, sizeof(m_ctx_surfaces_2d));
	std::memset(&m_kelvin, 0, sizeof(m_kelvin));
	std::memset(&m_img_blit, 0, sizeof(m_img_blit));
	// All classes are unbound at the beginning
	m_memcpy.m_instance_addr = UNBOUND_OBJ_ADDR;
	m_ctx_surfaces_2d.m_instance_addr = UNBOUND_OBJ_ADDR;
	m_kelvin.m_instance_addr = UNBOUND_OBJ_ADDR;
	m_img_blit.m_instance_addr = UNBOUND_OBJ_ADDR;
	m_img_blit.m_color_key_instance_addr = UNBOUND_OBJ_ADDR;
}

void pgraph::Impl::init(cpu *cpu, nv2a *gpu)
{
	m_pmc = gpu->getPmc();
	m_pramin = gpu->getPramin();
	m_lc86cpu = cpu->get86cpu();
	m_nv2a = gpu;
	reset();
	updateIo(false);

	m_ram = get_ram_ptr(m_lc86cpu);
	m_jthr = std::jthread(std::bind_front(&pgraph::Impl::graphHandler, this));
}

void pgraph::Impl::deinit()
{
	assert(m_jthr.joinable());
	m_jthr.request_stop();
	m_ctx_switch_trig.clear();
	m_ctx_switch_trig.notify_one();
	m_graph_has_work.test_and_set();
	m_graph_has_work.notify_one();
	m_jthr.join();
}

/** Public interface implementation **/
void pgraph::init(cpu *cpu, nv2a *gpu)
{
	m_impl->init(cpu, gpu);
}

void pgraph::deinit()
{
	m_impl->deinit();
}

void pgraph::reset()
{
	m_impl->reset();
}

void pgraph::updateIo()
{
	m_impl->updateIo();
}

uint32_t pgraph::read32(uint32_t addr)
{
	return m_impl->read32<false, on>(addr);
}

void pgraph::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false, on>(addr, value);
}

template<bool is_mthd_zero>
void pgraph::submitMethod(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t chid)
{
	m_impl->submitMethod<is_mthd_zero>(mthd, param, subchan, chid);
}

pgraph::pgraph() : m_impl{std::make_unique<pgraph::Impl>()} {}
pgraph::~pgraph() {}

template void pgraph::submitMethod<true>(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t chid);
template void pgraph::submitMethod<false>(uint32_t mthd, uint32_t param, uint32_t subchan, uint32_t chid);
