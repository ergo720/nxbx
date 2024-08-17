// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME smbus

#define SMBUS_IRQ_NUM 11
#define SMBUS_GS_addr 0xC000
#define SMBUS_GE_addr 0xC002
#define SMBUS_HA_addr 0xC004
#define SMBUS_HD_addr 0xC006
#define SMBUS_HC_addr 0xC008
#define SMBUS_GS_idx  0
#define SMBUS_GE_idx  1
#define SMBUS_HA_idx  2
#define SMBUS_HD_idx  3
#define SMBUS_HC_idx  4
					  
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


template<bool log>
uint8_t smbus::read8(uint32_t addr)
{
	uint8_t value;
	if (addr == SMBUS_GE_addr) {
		value = m_regs[SMBUS_GE_idx] & ~(GE_HOST_STC | GE_ABORT);
	}
	else {
		value = m_regs[(addr - SMBUS_GS_addr) >> 1];
	}

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

	switch (addr)
	{
	case SMBUS_GS_addr:
		if ((m_regs[SMBUS_GE_idx] & GE_HCYC_EN) && ((data & GS_CLEAR) & (m_regs[SMBUS_GS_idx] & GS_CLEAR))) {
			m_machine->lower_irq(SMBUS_IRQ_NUM);
		}

		for (uint8_t i = 0; i < 8; ++i) {
			if ((data & GS_CLEAR) & (1 << i)) {
				m_regs[SMBUS_GS_idx] &= ~(1 << i);
			}
		}
		break;

	case SMBUS_GE_addr:
		m_regs[SMBUS_GE_idx] |= (data & (GE_CYCTYPE | GE_HCYC_EN));
		if (data & GE_ABORT) {
			m_regs[SMBUS_GS_idx] |= GS_ABRT_STS;
			m_machine->raise_irq(SMBUS_IRQ_NUM);
			break;
		}
		else if (data & GE_HOST_STC) {
			start_cycle();
			if (m_regs[SMBUS_GS_idx] & GS_CLEAR) {
				m_machine->raise_irq(SMBUS_IRQ_NUM);
			}
		}
		break;

	case SMBUS_HA_addr:
	case SMBUS_HD_addr:
	case SMBUS_HC_addr:
		m_regs[(addr - SMBUS_GS_addr) >> 1] = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

void
smbus::start_cycle()
{
	m_regs[SMBUS_GS_idx] |= GS_PRERR_STS;
	// TODO
}

bool
smbus::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), 0xC000, 16, true,
		{
			.fnr8 = log ? cpu_read<smbus, uint8_t, &smbus::read8<true>> : cpu_read<smbus, uint8_t, &smbus::read8<false>>,
			.fnw8 = log ? cpu_write<smbus, uint8_t, &smbus::write8<true>> : cpu_write<smbus, uint8_t, &smbus::write8<false>>,
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
}

bool
smbus::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
