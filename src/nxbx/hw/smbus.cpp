// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "machine.hpp"
#include "smbus.hpp"
#include "eeprom.hpp"
#include "adm1032.hpp"
#include "smc.hpp"
#include "video/conexant.hpp"
#include "cpu.hpp"
#include <cstring>
#include <cinttypes>

#define MODULE_NAME smbus

#define SMBUS_IRQ_NUM 11
					  
#define GS_ABRT_STS   (1 << 0) // write one to clear
#define GS_COL_STS    (1 << 1) // write one to clear
#define GS_PRERR_STS  (1 << 2) // write one to clear
#define GS_HST_STS    (1 << 3) // read - only
#define GS_HCYC_STS   (1 << 4) // write one to clear
#define GS_TO_STS     (1 << 5) // write one to clear
#define GS_CLEAR      (GS_ABRT_STS | GS_COL_STS | GS_PRERR_STS | GS_HCYC_STS | GS_TO_STS)

#define GE_CYCTYPE    (7 << 0)
#define GE_HOST_STC   (1 << 3) // write - only
#define GE_HCYC_EN    (1 << 4)
#define GE_ABORT      (1 << 5) // write - only
#define GE_RW_BYTE    2
#define GE_RW_WORD    3
#define GE_RW_BLOCK   5

#define SMBUS_GS_addr 0xC000
#define SMBUS_GE_addr 0xC002
#define SMBUS_HA_addr 0xC004
#define SMBUS_HD0_addr 0xC006
#define SMBUS_HD1_addr 0xC007
#define SMBUS_HC_addr 0xC008
#define SMBUS_HB_addr 0xC009
#define SMBUS_REG_off(x) ((x) - SMBUS_GS_addr)


/** Private device implementation **/
class smbus::Impl
{
public:
	bool init(machine *machine);
	void deinit();
	void reset();
	void updateIoLogging() { updateIo(true); }
	template<bool log = false>
	uint8_t read8(uint32_t addr);
	template<bool log = false>
	uint16_t read16(uint32_t addr);
	template<bool log = false>
	void write8(uint32_t addr, const uint8_t value);
	template<bool log = false>
	void write16(uint32_t addr, const uint16_t value);

private:
	enum cycle_type {
		quick_command,
		byte_command,
		word_command,
		block_command,
	};

	bool updateIo(bool is_update);
	void start_cycle();
	template<cycle_type cmd, bool is_read, typename T = uint8_t>
	void end_cycle(smbus_device *dev, T value = 0);

	uint8_t m_regs[16];
	uint8_t m_block_data[32];
	unsigned m_block_off;
	std::unordered_map<uint8_t, smbus_device *> m_devs;
	// connected devices
	machine *m_machine;
	cpu_t *m_lc86cpu;
	// registers
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ SMBUS_GS_addr, "STATUS" },
		{ SMBUS_GE_addr, "CONTROL" },
		{ SMBUS_HA_addr, "ADDRESS" },
		{ SMBUS_HD0_addr, "DATA0" },
		{ SMBUS_HD1_addr, "DATA1" },
		{ SMBUS_HC_addr, "COMMAND" },
		{ SMBUS_HB_addr, "FIFO" },
	};
};

template<bool log>
uint8_t smbus::Impl::read8(uint32_t addr)
{
	uint8_t value;
	if (addr == SMBUS_GE_addr) {
		value = m_regs[SMBUS_REG_off(addr)] & ~(GE_HOST_STC | GE_ABORT);
	}
	else if (addr == SMBUS_HB_addr) {
		value = m_block_data[m_block_off];
		m_block_off++;
		m_block_off &= 0x1F;
	}
	else {
		value = m_regs[SMBUS_REG_off(addr)];
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
uint16_t smbus::Impl::read16(uint32_t addr)
{
	uint16_t value = read8(addr);
	value |= ((uint16_t)read8(addr + 1) << 8);

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void smbus::Impl::write8(uint32_t addr, const uint8_t value)
{
	if constexpr (log) {
		log_io_write();
	}

	uint8_t reg_off = SMBUS_REG_off(addr);
	switch (addr)
	{
	case SMBUS_GS_addr:
		if ((m_regs[SMBUS_REG_off(SMBUS_GE_addr)] & GE_HCYC_EN) && ((value & GS_CLEAR) & (m_regs[reg_off] & GS_CLEAR))) {
			m_machine->lower_irq(SMBUS_IRQ_NUM);
		}

		for (uint8_t i = 0; i < 8; ++i) {
			if ((value & GS_CLEAR) & (1 << i)) {
				m_regs[reg_off] &= ~(1 << i);
			}
		}
		break;

	case SMBUS_GE_addr:
		m_regs[reg_off] = value & (GE_CYCTYPE | GE_HCYC_EN);
		if (value & GE_ABORT) {
			m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_ABRT_STS;
		}
		else if (value & GE_HOST_STC) {
			start_cycle();
		}
		if (m_regs[SMBUS_REG_off(SMBUS_GE_addr)] & GE_HCYC_EN) {
			m_machine->raise_irq(SMBUS_IRQ_NUM);
		}
		break;

	case SMBUS_HA_addr:
	case SMBUS_HD0_addr:
	case SMBUS_HD1_addr:
	case SMBUS_HC_addr:
		m_regs[reg_off] = value;
		break;

	case SMBUS_HB_addr:
		m_block_data[m_block_off] = value;
		m_block_off++;
		m_block_off &= 0x1F;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log>
void smbus::Impl::write16(uint32_t addr, const uint16_t value)
{
	if constexpr (log) {
		log_io_write();
	}

	write8(addr, value & 0xFF);
	write8(addr + 1, value >> 8 & 0xFF);
}

template<smbus::Impl::cycle_type cmd, bool is_read, typename T>
void smbus::Impl::end_cycle(smbus_device *dev, T value)
{
	if (dev->has_cmd_succeeded()) {
		m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_HCYC_STS;
		if constexpr (cmd == quick_command) {
			return;
		}
		else if constexpr (cmd == byte_command) {
			if constexpr (is_read) {
				m_regs[SMBUS_REG_off(SMBUS_HD0_addr)] = value;
			}
		}
		else if constexpr (cmd == word_command) {
			if constexpr (is_read) {
				m_regs[SMBUS_REG_off(SMBUS_HD0_addr)] = value & 0xFF;
				m_regs[SMBUS_REG_off(SMBUS_HD1_addr)] = value >> 8;
			}
		}
		else if constexpr (cmd == block_command) {
			return;
		}
		else {
			throw std::logic_error("Out of range cycle type");
		}
	}
	else {
		if constexpr (cmd == block_command) {
			std::fill(std::begin(m_block_data), std::end(m_block_data), 0);
		}
		m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_PRERR_STS;
		dev->clear_cmd_status();
	}
}

void smbus::Impl::start_cycle()
{
	uint8_t hw_addr = m_regs[SMBUS_REG_off(SMBUS_HA_addr)] >> 1; // changes sw to hw address
	uint8_t is_read = m_regs[SMBUS_REG_off(SMBUS_HA_addr)] & 1;
	uint8_t command = m_regs[SMBUS_REG_off(SMBUS_HC_addr)];
	uint8_t data0 = m_regs[SMBUS_REG_off(SMBUS_HD0_addr)];
	uint8_t data1 = m_regs[SMBUS_REG_off(SMBUS_HD1_addr)];

	if (const auto it = m_devs.find(hw_addr); it != m_devs.end()) {
		switch (m_regs[SMBUS_REG_off(SMBUS_GE_addr)] & GE_CYCTYPE)
		{
		case 0:
			it->second->quick_command(is_read);
			end_cycle<quick_command, false>(it->second, is_read);
			break;

		case 1:
			if (is_read) {
				end_cycle<byte_command, true>(it->second, it->second->receive_byte());
			}
			else {
				it->second->send_byte(command);
				end_cycle<byte_command, false>(it->second);
			}
			break;

		case 2:
			if (is_read) {
				end_cycle<byte_command, true>(it->second, it->second->read_byte(command));
			}
			else {
				it->second->write_byte(command, data0);
				end_cycle<byte_command, false>(it->second);
			}
			break;

		case 3:
			if (is_read) {
				end_cycle<word_command, true>(it->second, it->second->read_word(command));
			}
			else {
				it->second->write_word(command, data0 | ((uint16_t)data1 << 8));
				end_cycle<word_command, false>(it->second);
			}
			break;

		case 4:
			end_cycle<word_command, true>(it->second, it->second->process_call(command, data0 | ((uint16_t)data1 << 8)));
			break;

		case 5:
			if (is_read) {
				uint8_t bytes_to_transfer = data0 > 32 ? 32 : data0;
				uint8_t start_off = m_block_off;
				for (uint8_t i = 0; i < bytes_to_transfer; ++i) {
					m_block_data[(start_off + i) & 0x1F] = it->second->read_byte(command + i);
				}
			}
			else {
				uint8_t bytes_to_transfer = data0 > 32 ? 32 : data0;
				uint8_t start_off = (m_block_off - bytes_to_transfer) & 0x1F;
				for (uint8_t i = 0; i < bytes_to_transfer; ++i) {
					it->second->write_byte(command + i, m_block_data[(start_off + i) & 0x1F]);
				}
			}
			end_cycle<block_command, false>(it->second); // NOTE: is_read doesn't matter for block_command
			break;
		}

		return;
	}

	// We reach here when an address specifies a non-existant device
	m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_PRERR_STS;
}

bool smbus::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, 0xC000, 16, true,
		{
			.fnr8 = log ? cpu_read<smbus::Impl, uint8_t, &smbus::Impl::read8<true>> : cpu_read<smbus::Impl, uint8_t, &smbus::Impl::read8<false>>,
			.fnr16 = log ? cpu_read<smbus::Impl, uint16_t, &smbus::Impl::read16<true>> : cpu_read<smbus::Impl, uint16_t, &smbus::Impl::read16<false>>,
			.fnw8 = log ? cpu_write<smbus::Impl, uint8_t, &smbus::Impl::write8<true>> : cpu_write<smbus::Impl, uint8_t, &smbus::Impl::write8<false>>,
			.fnw16 = log ? cpu_write<smbus::Impl, uint16_t, &smbus::Impl::write16<true>> : cpu_write<smbus::Impl, uint16_t, &smbus::Impl::write16<false>>,
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update smbus io ports");
		return false;
	}

	return true;
}

void smbus::Impl::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	std::fill(std::begin(m_block_data), std::end(m_block_data), 0);
	m_block_off = 0;
}

void smbus::Impl::deinit()
{
	for (auto dev : m_devs) {
		dev.second->deinit();
	}
}

bool smbus::Impl::init(machine *machine)
{
	m_lc86cpu = machine->get86cpu();
	m_machine = machine;
	if (!updateIo(false)) {
		return false;
	}

	m_devs[0x54] = m_machine->getEeprom(); // eeprom
	m_devs[0x10] = m_machine->getSmc(); // smc
	m_devs[0x4C] = m_machine->getAdm1032(); // adm1032
	m_devs[0x45] = m_machine->getVideoEncoder(); // conexant video encoder
	reset();
	return true;
}

/** Public interface implementation **/
bool smbus::init(machine *machine)
{
	return m_impl->init(machine);
}

void smbus::deinit()
{
	m_impl->deinit();
}

void smbus::reset()
{
	m_impl->reset();
}

void smbus::updateIoLogging()
{
	m_impl->updateIoLogging();
}

smbus::smbus() : m_impl{std::make_unique<smbus::Impl>()} {}
smbus::~smbus() {}
