// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2025 ergo720

#include "lib86cpu.hpp"
#include "machine.hpp"
#include "ohci.hpp"
#include "cpu.hpp"
#include "clock.hpp"
#include "util.hpp"
#include "host.hpp"
#include <algorithm>
#include <cassert>

#define MODULE_NAME usb0

#define USB0_IRQ_NUM 1

#define USB0_BASE 0xFED00000
#define USB0_SIZE 0x1000
#define REGS_USB0_idx(x) ((x - USB0_BASE) >> 2)
#define REG_USB0(r) (m_regs[REGS_USB0_idx(r)])

#include "ohci_reg_defs.hpp"


struct port_status {
	uint32_t rh_port_status;
	unsigned idx;
};

class usb0::Impl
{
public:
	bool init(machine *machine);
	void reset();
	void updateIoLogging() { updateIo(true); }
	template<bool log>
	uint32_t read(uint32_t addr);
	template<bool log>
	void write(uint32_t addr, const uint32_t value);
	uint64_t getNextUpdateTime(uint64_t now);

private:
	enum class state : uint32_t {
		reset = 0,
		resume = 1,
		operational = 2,
		suspend = 3
	};
	static constexpr uint32_t state_reset = std::to_underlying(state::reset);
	static constexpr uint32_t state_resume = std::to_underlying(state::resume);
	static constexpr uint32_t state_operational = std::to_underlying(state::operational);
	static constexpr uint32_t state_suspend = std::to_underlying(state::suspend);
	static constexpr uint64_t m_usb_freq = 12000000; // 12 MHz
	bool updateIo(bool is_update);
	template<typename T>
	void update_port_status(T &&f);
	void update_state(uint32_t value);
	void set_int(uint32_t value);
	void update_int();
	void eof_worker();
	uint32_t calc_frame_left();
	void hw_reset();
	void sw_reset();

	bool m_frame_running;
	uint64_t m_sof_time; // time of the sof token, that is, when a new frame starts
	// connected devices
	machine *m_machine;
	cpu_t *m_lc86cpu;
	// registers
	port_status m_port[4];
	uint32_t m_regs[USB0_SIZE / 4];
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ REVISION, "REVISION" },
		{ CTRL, "CONTROL" },
		{ CMD_ST, "COMMAND_STATUS" },
		{ INT_ST, "INTERRUPT_STATUS" },
		{ INT_EN, "INTERRUPT_ENABLE"},
		{ INT_DIS, "INTERRUPT_DISABLE"},
		{ HCCA, "HCCA" },
		{ PERIOD_CURR_ED, "PERIOD_CURR_ED" },
		{ CTRL_HEAD_ED, "CONTROL_HEAD_ED" },
		{ CTRL_CURR_ED, "CONTROL_CURRENT_ED" },
		{ BULK_HEAD_ED, "BULK_HEAD_ED" },
		{ BULK_CURR_ED, "BULK_CURRENT_ED" },
		{ DONE_HEAD, "DONE_HEAD" },
		{ FM_INTERVAL, "FRAME_INTERVAL" },
		{ FM_REMAINING, "FRAME_REMAINING" },
		{ FM_NUM, "FRAME_NUM" },
		{ PERIOD_START, "PERIODIC_START" },
		{ LS_THRESHOLD, "LS_THRESHOLD" },
		{ RH_DESCRIPTOR_A, "RHDESCRIPTORA"},
		{ RH_DESCRIPTOR_B, "RHDESCRIPTORB"},
		{ RH_ST, "RHSTATUS"},
		{ RH_PORT_ST(0), "RHPORTSTATUS0" },
		{ RH_PORT_ST(1), "RHPORTSTATUS1" },
		{ RH_PORT_ST(2), "RHPORTSTATUS2" },
		{ RH_PORT_ST(3), "RHPORTSTATUS3" },
	};
};


template<typename T>
void usb0::Impl::update_port_status(T &&f)
{
	std::for_each(std::begin(m_port), std::end(m_port), f);
}

template<bool log>
void usb0::Impl::write(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		log_io_write();
	}

	switch (addr)
	{
	case REVISION:
	case PERIOD_CURR_ED:
	case DONE_HEAD:
	case FM_REMAINING:
	case FM_NUM:
		// read-only
		break;

	case CTRL:
		update_state(value);
		break;

	case CMD_ST:
		REG_USB0(CMD_ST) |= (value & ~CMD_ST_RO_MASK);
		if (value & CMD_ST_HCR) {
			sw_reset();
		}
		break;

	case INT_ST:
		REG_USB0(INT_ST) &= ~value;
		update_int();
		break;

	case INT_EN:
		REG_USB0(INT_EN) |= value;
		update_int();
		break;

	case INT_DIS:
		REG_USB0(INT_EN) &= ~value;
		update_int();
		break;

	case HCCA:
		REG_USB0(HCCA) = (value & ~HCCA_RO_MASK);
		break;

	case CTRL_HEAD_ED:
	case CTRL_CURR_ED:
	case BULK_HEAD_ED:
	case BULK_CURR_ED:
		REG_USB0(addr) = (value & ~ED_RO_MASK);
		break;

	case RH_DESCRIPTOR_A:
		REG_USB0(RH_DESCRIPTOR_A) = (value & ~RHDA_RO_MASK) | (REG_USB0(RH_DESCRIPTOR_A) & RHDA_RO_MASK);
		break;

	case RH_ST:
		if (value & RH_ST_LPS) { // ClearGlobalPower
			if ((REG_USB0(RH_DESCRIPTOR_A) & (RHDA_NPS | RHDA_PSM)) == 0) {
				// global power mode: turn off power to all ports
				update_port_status([](port_status &p){ p.rh_port_status &= ~RH_PORT_ST_PPS; });
			}
			else if ((REG_USB0(RH_DESCRIPTOR_A) & (RHDA_NPS | RHDA_PSM)) == RHDA_PSM) {
				// per-port power mode: turn off power for ports that have RHDB_PPCM cleared
				update_port_status([this](port_status &p)
					{
						if ((REG_USB0(RH_DESCRIPTOR_B) & RHDB_PPCM(p.idx)) == 0) {
							p.rh_port_status &= ~RH_PORT_ST_PPS;
						}
					});
			}
		}
		if (value & RH_ST_LPSC) { // SetGlobalPower
			if ((REG_USB0(RH_DESCRIPTOR_A) & (RHDA_NPS | RHDA_PSM)) == 0) {
				// global power mode: turn on power to all ports
				update_port_status([](port_status &p){ p.rh_port_status |= RH_PORT_ST_PPS; });
			}
			else if ((REG_USB0(RH_DESCRIPTOR_A) & (RHDA_NPS | RHDA_PSM)) == RHDA_PSM) {
				// per-port power mode: turn on power for ports that have RHDB_PPCM cleared
				update_port_status([this](port_status &p)
					{
						if ((REG_USB0(RH_DESCRIPTOR_B) & RHDB_PPCM(p.idx)) == 0) {
							p.rh_port_status |= RH_PORT_ST_PPS;
						}
					});
			}
		}
		if (value & RH_ST_DRWE) { // SetRemoteWakeupEnable
			REG_USB0(RH_ST) |= RH_ST_DRWE;
		}
		if (value & RH_ST_CRWE) { // ClearRemoteWakeupEnable
			REG_USB0(RH_ST) &= ~RH_ST_DRWE;
		}
		break;

	default:
		REG_USB0(addr) = value;
	}
}

template<bool log>
uint32_t usb0::Impl::read(uint32_t addr)
{
	uint32_t value;

	switch (addr)
	{
	case INT_DIS:
		value = REG_USB0(INT_EN);
		break;

	case FM_REMAINING:
		value = calc_frame_left();
		break;

	default:
		value = REG_USB0(addr);
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

uint32_t usb0::Impl::calc_frame_left()
{
	if (((REG_USB0(CTRL) & CTRL_HCFS) >> 6) != state_operational) {
		return REG_USB0(FM_REMAINING); // frame time only runs in operational state
	}

	// NOTE: usb time here must be relative to the last sof time, not the boot time as used by get_dev_now
	uint64_t curr_time = (timer::get_now() - m_sof_time) % timer::g_ticks_per_millisecond;
	curr_time = util::muldiv128(curr_time, m_usb_freq, timer::g_ticks_per_second);
	assert((curr_time & ~FM_INTERVAL_FI) == 0);
	uint32_t frame_time = (REG_USB0(FM_INTERVAL) & FM_INTERVAL_FI) - (curr_time & FM_INTERVAL_FI);

	return (REG_USB0(FM_REMAINING) & FM_REMAINING_FRT) | frame_time;
}

void usb0::Impl::update_state(uint32_t value)
{
	uint32_t old_state = (REG_USB0(CTRL) & CTRL_HCFS) >> 6;
	uint32_t new_state = (value & CTRL_HCFS) >> 6;
	REG_USB0(CTRL) = value;

	if (new_state != old_state) {
		switch (new_state)
		{
		case state_reset:
			hw_reset();
			break;

		case state_resume:
			m_frame_running = false;
			logger_en(debug, "Resume state");
			break;

		case state_operational:
			m_sof_time = timer::get_now();
			m_frame_running = true;
			REG_USB0(FM_REMAINING) = ((REG_USB0(FM_REMAINING) & FM_REMAINING_FRT) | (REG_USB0(FM_INTERVAL) & FM_INTERVAL_FI));
			set_int(INT_SF);
			logger_en(debug, "Operational state");
			break;

		case state_suspend:
			m_frame_running = false;
			logger_en(debug, "Suspend state");
			break;

		default:
			std::unreachable();
		}
	}
}

void usb0::Impl::set_int(uint32_t value)
{
	REG_USB0(INT_ST) |= value;
	update_int();
}

void usb0::Impl::update_int()
{
	uint32_t mie_en = REG_USB0(INT_EN) & INT_MIE;
	uint32_t int_en = REG_USB0(INT_EN) & INT_ALL;
	uint32_t int_pending = REG_USB0(INT_ST) & INT_ALL;

	if (mie_en && (int_pending & int_en)) {
		m_machine->raise_irq(USB0_IRQ_NUM);
	}
	else {
		m_machine->lower_irq(USB0_IRQ_NUM);
	}
}

void usb0::Impl::eof_worker()
{
	// TODO
}

void usb0::Impl::sw_reset()
{
	std::fill_n(std::begin(m_regs), (RH_DESCRIPTOR_A - USB0_BASE) / 4, 0);
	REG_USB0(REVISION) = 0x10;
	REG_USB0(FM_INTERVAL) = 0x2EDF | (0x2778 << 16);
	REG_USB0(LS_THRESHOLD) = 0x628;
	REG_USB0(CTRL) |= (state_suspend << 6);
	m_frame_running = false;

	logger_en(debug, "Suspend state");
}

void usb0::Impl::hw_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	REG_USB0(REVISION) = 0x10;
	REG_USB0(FM_INTERVAL) = 0x2EDF | (0x2778 << 16);
	REG_USB0(LS_THRESHOLD) = 0x628;
	REG_USB0(RH_DESCRIPTOR_A) = RHDA_NPS | RHDA_NOCP | 4; // four ports for HC
	m_frame_running = false;

	logger_en(debug, "Reset state");
}

uint64_t usb0::Impl::getNextUpdateTime(uint64_t now)
{
	if (m_frame_running) {
		uint64_t next_time;
		if ((now - m_sof_time) >= timer::g_ticks_per_millisecond) { // frame length of ohci is 1 ms
			m_sof_time = now;
			next_time = timer::g_ticks_per_millisecond;
			eof_worker();
		}
		else {
			next_time = m_sof_time + timer::g_ticks_per_millisecond - now;
		}

		return next_time;
	}

	return std::numeric_limits<uint64_t>::max();
}

bool usb0::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, USB0_BASE, USB0_SIZE, false,
		{
			.fnr32 = log ? cpu_read<usb0::Impl, uint32_t, &usb0::Impl::read<true>> : cpu_read<usb0::Impl, uint32_t, &usb0::Impl::read<false>>,
			.fnw32 = log ? cpu_write<usb0::Impl, uint32_t, &usb0::Impl::write<true>> : cpu_write<usb0::Impl, uint32_t, &usb0::Impl::write<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void usb0::Impl::reset()
{
	hw_reset();
}

bool usb0::Impl::init(machine *machine)
{
	m_lc86cpu = machine->get86cpu();
	m_machine = machine;
	if (!updateIo(false)) {
		return false;
	}

	reset();
	return true;
}

/** Public interface implementation **/
bool usb0::init(machine *machine)
{
	return m_impl->init(machine);
}

void usb0::reset()
{
	m_impl->reset();
}

void usb0::updateIoLogging()
{
	m_impl->updateIoLogging();
}

uint64_t usb0::getNextUpdateTime(uint64_t now)
{
	return m_impl->getNextUpdateTime(now);
}

usb0::usb0() : m_impl{std::make_unique<usb0::Impl>()} {}
usb0::~usb0() {}
