// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2025 ergo720

#include "machine.hpp"
#include <algorithm>

#define MODULE_NAME usb0

#define CTRL_HCFS (3 << 6) // HostControllerFunctionalState
#define CMD_ST_HCR (1 << 0) // SchedulingOverrunCount
#define CMD_ST_SOC (3 << 16) // SchedulingOverrunCount
#define CMD_ST_RO_MASK CMD_ST_SOC
#define RHDA_NDP 0xFF // NumberDownstreamPorts
#define RHDA_PSM (1 << 8) // PowerSwitchingMode
#define RHDA_NPS (1 << 9) // NoPowerSwitching
#define RHDA_DT (1 << 10) // DeviceType
#define RHDA_NOCP (1 << 12) // NoOverCurrentProtection
#define RHDA_RO_MASK (RHDA_DT | RHDA_NDP)
#define RHDB_PPCM(i) (1 << (17 + i)) // PortPowerControlMask
#define RH_ST_LPS (1 << 0) // LocalPowerStatus, ClearGlobalPower
#define RH_ST_OCI (1 << 1) // OverCurrentIndicator
#define RH_ST_DRWE (1 << 15) // DeviceRemoteWakeupEnable
#define RH_ST_LPSC (1 << 16) // LocalPowerStatusChange, SetGlobalPower
#define RH_ST_CRWE (1 << 31) // ClearRemoteWakeupEnable
#define RH_PORT_ST_PPS (1 << 8) // PortPowerStatus


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
		// read-only
		break;

	case CMD_ST:
		REG_USB0(CMD_ST) |= (value & ~CMD_ST_RO_MASK);
		if (value & CMD_ST_HCR) {
			sw_reset();
		}
		break;

	case RH_DESCRIPTOR_A:
		REG_USB0(RH_DESCRIPTOR_A) = (value & ~RHDA_RO_MASK) | (REG_USB0(RH_DESCRIPTOR_A) & RHDA_RO_MASK);
		break;

	case RH_ST:
		if (value & RH_ST_LPS) { // ClearGlobalPower
			if ((REG_USB0(RH_DESCRIPTOR_A) & (RHDA_NPS | RHDA_PSM)) == 0) {
				// global power mode: turn off power to all ports
				update_port_status([](usb_port &p){ p.rh_port_status &= ~RH_PORT_ST_PPS; });
			}
			else if ((REG_USB0(RH_DESCRIPTOR_A) & (RHDA_NPS | RHDA_PSM)) == RHDA_PSM) {
				// per-port power mode: turn off power for ports that have RHDB_PPCM cleared
				update_port_status([this](usb_port &p)
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
				update_port_status([](usb_port &p){ p.rh_port_status |= RH_PORT_ST_PPS; });
			}
			else if ((REG_USB0(RH_DESCRIPTOR_A) & (RHDA_NPS | RHDA_PSM)) == RHDA_PSM) {
				// per-port power mode: turn on power for ports that have RHDB_PPCM cleared
				update_port_status([this](usb_port &p)
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
	uint32_t value = REG_USB0(addr);

	if constexpr (log) {
		log_io_read();
	}

	return value;
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
usb0::sw_reset()
{
	std::fill_n(std::begin(m_regs), (RH_DESCRIPTOR_A - USB0_BASE) / 4, 0);
	REG_USB0(REVISION) = 0x10;

	REG_USB0(CTRL) |= ((std::to_underlying(usb_state::suspend) << 6) & CTRL_HCFS);

	logger_en(debug, "Suspend mode");
}

void
usb0::hw_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	REG_USB0(REVISION) = 0x10;
	REG_USB0(RH_DESCRIPTOR_A) = RHDA_NPS | RHDA_NOCP | 4; // four ports for HC
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
