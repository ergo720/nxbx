// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"
#include <cstring>

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


template<bool log>
uint8_t smbus::read8(uint32_t addr)
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
uint16_t smbus::read16(uint32_t addr)
{
	uint16_t value = read8(addr);
	value |= ((uint16_t)read8(addr + 1) << 8);

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

template<bool log>
void smbus::write8(uint32_t addr, const uint8_t value)
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
void smbus::write16(uint32_t addr, const uint16_t value)
{
	if constexpr (log) {
		log_io_write();
	}

	write8(addr, value & 0xFF);
	write8(addr + 1, value >> 8 & 0xFF);
}

template<smbus::cycle_type cmd, bool is_read, typename T>
void smbus::end_cycle(smbus_device *dev, T value)
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

void
smbus::start_cycle()
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

bool
smbus::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0xC000, 16, true,
		{
			.fnr8 = log ? cpu_read<smbus, uint8_t, &smbus::read8<true>> : cpu_read<smbus, uint8_t, &smbus::read8<false>>,
			.fnr16 = log ? cpu_read<smbus, uint16_t, &smbus::read16<true>> : cpu_read<smbus, uint16_t, &smbus::read16<false>>,
			.fnw8 = log ? cpu_write<smbus, uint8_t, &smbus::write8<true>> : cpu_write<smbus, uint8_t, &smbus::write8<false>>,
			.fnw16 = log ? cpu_write<smbus, uint16_t, &smbus::write16<true>> : cpu_write<smbus, uint16_t, &smbus::write16<false>>,
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update smbus io ports");
		return false;
	}

	return true;
}

void
smbus::reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	std::fill(std::begin(m_block_data), std::end(m_block_data), 0);
	m_block_off = 0;
}

void
smbus::deinit()
{
	for (auto dev : m_devs) {
		dev.second->deinit();
	}
}

bool
smbus::init()
{
	if (!update_io(false)) {
		return false;
	}

	m_devs[0x54] = &m_machine->get<eeprom>(); // eeprom
	m_devs[0x10] = &m_machine->get<smc>(); // smc
	m_devs[0x4C] = &m_machine->get<adm1032>(); // adm1032
	m_devs[0x45] = &m_machine->get<conexant>(); // conexant video encoder
	reset();
	return true;
}
