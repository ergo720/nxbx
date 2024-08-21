// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"
#include <cstring>

#define MODULE_NAME smbus

#define SMBUS_IRQ_NUM 11
#define SMBUS_GS_addr 0xC000
#define SMBUS_GE_addr 0xC002
#define SMBUS_HA_addr 0xC004
#define SMBUS_HD0_addr 0xC006
#define SMBUS_HD1_addr 0xC007
#define SMBUS_HC_addr 0xC008
#define SMBUS_HB_addr 0xC009
#define SMBUS_REG_off(x) ((x) - SMBUS_GS_addr)
					  
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
void smbus::write8(uint32_t addr, const uint8_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	uint8_t reg_off = SMBUS_REG_off(addr);
	switch (addr)
	{
	case SMBUS_GS_addr:
		if ((m_regs[SMBUS_REG_off(SMBUS_GE_addr)] & GE_HCYC_EN) && ((data & GS_CLEAR) & (m_regs[reg_off] & GS_CLEAR))) {
			m_machine->lower_irq(SMBUS_IRQ_NUM);
		}

		for (uint8_t i = 0; i < 8; ++i) {
			if ((data & GS_CLEAR) & (1 << i)) {
				m_regs[reg_off] &= ~(1 << i);
			}
		}
		break;

	case SMBUS_GE_addr:
		m_regs[reg_off] |= (data & (GE_CYCTYPE | GE_HCYC_EN));
		if (data & GE_ABORT) {
			m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_ABRT_STS;
			m_machine->raise_irq(SMBUS_IRQ_NUM);
			break;
		}
		else if (data & GE_HOST_STC) {
			start_cycle();
			m_machine->raise_irq(SMBUS_IRQ_NUM);
		}
		break;

	case SMBUS_HA_addr:
	case SMBUS_HD0_addr:
	case SMBUS_HD1_addr:
	case SMBUS_HC_addr:
		m_regs[reg_off] = data;
		break;

	case SMBUS_HB_addr:
		m_block_data[m_block_off] = data;
		m_block_off++;
		m_block_off &= 0x1F;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log>
void smbus::write16(uint32_t addr, const uint16_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	write8(addr, data & 0xFF);
	write8(addr + 1, data >> 8 & 0xFF);
}

void
smbus::start_cycle()
{
	uint8_t hw_addr = m_regs[SMBUS_REG_off(SMBUS_HA_addr)] >> 1; // changes sw to hw address
	uint8_t is_read = m_regs[SMBUS_REG_off(SMBUS_HA_addr)] & 1;
	uint8_t command = m_regs[SMBUS_REG_off(SMBUS_HC_addr)];
	uint8_t data0 = m_regs[SMBUS_REG_off(SMBUS_HD0_addr)];
	uint8_t data1 = m_regs[SMBUS_REG_off(SMBUS_HD1_addr)];

	const auto &check_success = [this, is_read]<int cycle_type>(std::optional<uint16_t> ret)
	{
		if constexpr (cycle_type == 0) {
			m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= (ret == std::nullopt ? GS_PRERR_STS : GS_HCYC_STS);
		}
		else if constexpr ((cycle_type == 1) || (cycle_type == 2)) {
			if (ret == std::nullopt) {
				m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_PRERR_STS;
			}
			else {
				if (is_read) {
					m_regs[SMBUS_REG_off(SMBUS_HD0_addr)] = *ret;
				}
				m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_HCYC_STS;
			}
		}
		else if constexpr ((cycle_type == 3) || cycle_type == 4) {
			if (ret == std::nullopt) {
				m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_PRERR_STS;
			}
			else {
				if (is_read) {
					m_regs[SMBUS_REG_off(SMBUS_HD0_addr)] = (*ret) & 0xFF;
					m_regs[SMBUS_REG_off(SMBUS_HD1_addr)] = (*ret) >> 8;
				}
				m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_HCYC_STS;
			}
		}
		else {
			nxbx_fatal("Out of range cycle type");
		}
	};

	if (const auto it = m_devs.find(hw_addr); it != m_devs.end()) {
		std::optional<uint16_t> ret;

		switch (m_regs[SMBUS_REG_off(SMBUS_GE_addr)] & GE_CYCTYPE)
		{
		case 0:
			ret = it->second->quick_command(is_read);
			check_success.template operator()<0>(ret);
			return;

		case 1:
			if (is_read) {
				ret = it->second->receive_byte();
			}
			else {
				ret = it->second->send_byte(command);
			}
			check_success.template operator()<1>(ret);
			return;

		case 2:
			if (is_read) {
				ret = it->second->read_byte(command);
			}
			else {
				ret = it->second->write_byte(command, data0);
			}
			check_success.template operator()<2>(ret);
			return;

		case 3:
			if (is_read) {
				ret = it->second->read_word(command);
			}
			else {
				ret = it->second->write_word(command, data0 | ((uint16_t)data1 << 8));
			}
			check_success.template operator()<3>(ret);
			return;

		case 4:
			ret = it->second->process_call(command, data0 | ((uint16_t)data1 << 8));
			check_success.template operator()<4>(ret);
			return;

		case 5:
			if (is_read) {
				uint8_t bytes_to_transfer = data0 > 32 ? 32 : data0;
				uint8_t start_off = m_block_off;
				for (uint8_t i = 0; i < bytes_to_transfer; ++i) {
					if (const auto opt = it->second->read_byte(command + i); opt) {
						m_block_data[(start_off + i) & 0x1F] = *opt;
						continue;
					}
					std::memset(m_block_data, 0, 32);
					m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_PRERR_STS;
					return;
				}
			}
			else {
				uint8_t bytes_to_transfer = data0 > 32 ? 32 : data0;
				uint8_t start_off = (m_block_off - bytes_to_transfer) & 0x1F;
				for (uint8_t i = 0; i < bytes_to_transfer; ++i) {
					if (!it->second->write_byte(command + i, m_block_data[(start_off + i) & 0x1F])) {
						std::memset(m_block_data, 0, 32);
						m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_PRERR_STS;
						return;
					}
				}
			}
			m_regs[SMBUS_REG_off(SMBUS_GS_addr)] |= GS_HCYC_STS;
			return;
		}
	}

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
	std::fill(&m_regs[0], &m_regs[4], 0);
	std::fill(&m_block_data[0], &m_block_data[4], 0);
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
	m_devs[0x4C] = &m_machine->get<adm>(); // adm
	reset();
	return true;
}
