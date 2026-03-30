// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"
#include "smc.hpp"
#include"adm1032.hpp"
#include "host.hpp"
#include <cinttypes>
#include <atomic>

#define MODULE_NAME smc

#define SMC_VERSION_STR                 0x01
#define SMC_TRAY_STATE                  0x03
#define SMC_VIDEO_MODE                  0x04
#define SMC_FAN_MODE                    0x05
#define SMC_FAN_SPEED                   0x06
#define SMC_LED_OVERRIDE                0x07
#define SMC_LED_STATES                  0x08
#define SMC_CPU_TEMPERATURE             0x09
#define SMC_MB_TEMPERATURE              0x0A
#define SMC_WRITE_SCRATCH               0x0E
#define SMC_READ_SCRATCH                0x0F
#define SMC_READ_FAN_SPEED              0x10
#define SMC_SCRATCH                     0x1B

#define SMC_TRAY_STATE_OPEN             0x10
#define SMC_TRAY_STATE_NO_MEDIA         0x40
#define SMC_TRAY_STATE_MEDIA_DETECT     0x60

#define SMC_VIDEO_MODE_SCART            0x00
#define SMC_VIDEO_MODE_HDTV             0x01
#define SMC_VIDEO_MODE_VGA              0x02
#define SMC_VIDEO_MODE_RFU              0x03
#define SMC_VIDEO_MODE_SVIDEO           0x04
#define SMC_VIDEO_MODE_STANDARD         0x06
#define SMC_VIDEO_MODE_NONE             0x07


/** Private device implementation **/
class smc::Impl
{
public:
	bool init(machine *machine);
	void reset();
	uint8_t read_byte(uint8_t addr);
	void write_byte(uint8_t addr, uint8_t value);
	void update_tray_state(tray_state state, bool do_int);

private:
	static constexpr uint8_t m_version[3] = { 'P', '0', '5' };
	uint8_t m_version_idx;
	uint8_t m_regs[34];
	std::atomic_uint8_t m_tray_state; // atomic because it can be updated by console::update_tray_state
	// connected devices
	adm1032 *m_adm1032;
	// registers
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ SMC_VERSION_STR, "VERSION_STR" },
		{ SMC_TRAY_STATE, "TRAY_STATE" },
		{ SMC_VIDEO_MODE, "VIDEO_MODE" },
		{ SMC_FAN_MODE, "FAN_MODE" },
		{ SMC_FAN_SPEED, "FAN_SPEED" },
		{ SMC_LED_OVERRIDE, "LED_OVERRIDE" },
		{ SMC_LED_STATES, "LED_STATES" },
		{ SMC_CPU_TEMPERATURE, "CPU_TEMPERATURE" },
		{ SMC_MB_TEMPERATURE, "MB_TEMPERATURE" },
		{ SMC_WRITE_SCRATCH, "WRITE_SCRATCH" },
		{ SMC_READ_SCRATCH, "READ_SCRATCH" },
		{ SMC_READ_FAN_SPEED, "READ_FAN_SPEED" },
		{ SMC_SCRATCH, "SCRATCH" },
	};
};

uint8_t smc::Impl::read_byte(uint8_t addr)
{
	uint8_t value;
	switch (addr)
	{
	case SMC_VERSION_STR:
		value = m_version[m_version_idx];
		m_version_idx = (m_version_idx + 1) % 3;
		break;

	case SMC_TRAY_STATE:
		value = m_tray_state.load();
		break;

	case SMC_VIDEO_MODE:
	case SMC_SCRATCH:
		value = m_regs[addr];
		break;

	case SMC_CPU_TEMPERATURE:
	case SMC_MB_TEMPERATURE:
		value = m_adm1032->read_byte((addr - SMC_CPU_TEMPERATURE) ^ 1);
		break;

	case SMC_READ_SCRATCH:
		value = m_regs[SMC_WRITE_SCRATCH];
		break;

	case SMC_READ_FAN_SPEED:
		value = (m_regs[SMC_FAN_MODE] == 1) ? m_regs[SMC_FAN_SPEED] : 0;
		break;

	default:
		nxbx_fatal("Unhandled read with command 0x%" PRIX8, addr);
		value = 0;
	}

	log_io_read_en();

	return value;
}

void smc::Impl::write_byte(uint8_t addr, uint8_t value)
{
	log_io_write_en();

	switch (addr)
	{
	case SMC_VERSION_STR:
		m_version_idx = (value == 0) ? 0 : m_version_idx;
		break;

	case SMC_FAN_MODE:
		m_regs[addr] = value & 1;
		break;

	case SMC_FAN_SPEED:
		m_regs[addr] = (value > 50) ? m_regs[addr] : value;
		break;

	case SMC_LED_OVERRIDE: // TODO: display on the gui somehow
		m_regs[addr] = value & 1;
		if (m_regs[addr] == 0) {
			m_regs[SMC_LED_STATES] = 0x0F; // solid green
		}
		break;

	case SMC_LED_STATES:
		m_regs[addr] = value;
		break;

	default:
		nxbx_fatal("Unhandled write with command 0x%" PRIX8 " and value 0x%" PRIX8, addr, value);
	}
}

void smc::Impl::update_tray_state(::tray_state state, bool do_int)
{
	m_tray_state = (uint8_t)state;
	if (do_int) {
		// TODO: trigger interrupt
		nxbx_fatal("Tray interrupts not supported yet");
	}
}

void smc::Impl::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_regs[SMC_LED_STATES] = 0x0F; // solid green
	m_version_idx = 0;
}

bool smc::Impl::init(machine *machine)
{
	m_adm1032 = machine->getAdm1032();
	reset();
	m_tray_state = SMC_TRAY_STATE_MEDIA_DETECT; // TODO: should change state when the user boots new XBEs/XISOs from the gui
	m_regs[SMC_VIDEO_MODE] = SMC_VIDEO_MODE_HDTV; // TODO: make configurable

	return true;
}

/** Public interface implementation **/
bool smc::init(machine *machine, log_module log_module)
{
	m_log_module = log_module;
	return m_impl->init(machine);
}

void smc::deinit()
{
	// empty
}

uint8_t smc::read_byte(uint8_t command)
{
	return m_impl->read_byte(command);
}

void smc::write_byte(uint8_t command, uint8_t value)
{
	m_impl->write_byte(command, value);
}

void smc::update_tray_state(tray_state state, bool do_int)
{
	m_impl->update_tray_state(state, do_int);
}

smc::smc() : m_impl{std::make_unique<smc::Impl>()} {}
smc::~smc() {}
