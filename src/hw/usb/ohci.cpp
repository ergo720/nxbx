// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2025 ergo720

#include "machine.hpp"
#include "../clock.hpp"
#include <algorithm>

#define MODULE_NAME usb0

#define USB0_IRQ_NUM 1


template<typename T>
void usb0::update_port_status(T &&f)
{
	std::for_each(std::begin(m_port), std::end(m_port), f);
}

template<bool log>
void usb0::write(uint32_t addr, const uint32_t value)
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
uint32_t usb0::read(uint32_t addr)
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

uint32_t
usb0::calc_frame_left()
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

void
usb0::update_state(uint32_t value)
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

void
usb0::set_int(uint32_t value)
{
	REG_USB0(INT_ST) |= value;
	update_int();
}

void
usb0::update_int()
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

void
usb0::eof_worker()
{
	// TODO
}

void
usb0::sw_reset()
{
	std::fill_n(std::begin(m_regs), (RH_DESCRIPTOR_A - USB0_BASE) / 4, 0);
	REG_USB0(REVISION) = 0x10;
	REG_USB0(FM_INTERVAL) = 0x2EDF | (0x2778 << 16);
	REG_USB0(LS_THRESHOLD) = 0x628;
	REG_USB0(CTRL) |= (state_suspend << 6);
	m_frame_running = false;

	logger_en(debug, "Suspend state");
}

void
usb0::hw_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	REG_USB0(REVISION) = 0x10;
	REG_USB0(FM_INTERVAL) = 0x2EDF | (0x2778 << 16);
	REG_USB0(LS_THRESHOLD) = 0x628;
	REG_USB0(RH_DESCRIPTOR_A) = RHDA_NPS | RHDA_NOCP | 4; // four ports for HC
	m_frame_running = false;

	logger_en(debug, "Reset state");
}

uint64_t
usb0::get_next_update_time(uint64_t now)
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

bool
usb0::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), USB0_BASE, USB0_SIZE, false,
		{
			.fnr32 = log ? cpu_read<usb0, uint32_t, &usb0::read<true>> : cpu_read<usb0, uint32_t, &usb0::read<false>>,
			.fnw32 = log ? cpu_write<usb0, uint32_t, &usb0::write<true>> : cpu_write<usb0, uint32_t, &usb0::write<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
usb0::reset()
{
	hw_reset();
}

bool
usb0::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
