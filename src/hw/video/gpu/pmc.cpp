// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "machine.hpp"

#define MODULE_NAME pmc

#define NV_PMC 0x00000000
#define NV_PMC_BASE (NV2A_REGISTER_BASE + NV_PMC)
#define NV_PMC_SIZE 0x1000

#define NV_PMC_BOOT_0 (NV2A_REGISTER_BASE + 0x00000000)
#define NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0 0x02A000A3
#define NV_PMC_BOOT_1 (NV2A_REGISTER_BASE + 0x00000004)
#define NV_PMC_BOOT_1_ENDIAN00_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN00_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE 0x00000000
#define NV_PMC_BOOT_1_ENDIAN24_BIG 0x00000001
#define NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK (0x00000000 << 0)
#define NV_PMC_BOOT_1_ENDIAN0_BIG_MASK (0x00000001 << 0)
#define NV_PMC_BOOT_1_ENDIAN24_LITTLE_MASK (0x00000000 << 24)
#define NV_PMC_BOOT_1_ENDIAN24_BIG_MASK (0x00000001 << 24)
#define NV_PMC_INTR_0 (NV2A_REGISTER_BASE + 0x00000100)
#define NV_PMC_INTR_0_PTIMER 20
#define NV_PMC_INTR_0_PCRTC 24
#define NV_PMC_INTR_0_SOFTWARE 31
#define NV_PMC_INTR_0_NOT_PENDING 0x00000000
#define NV_PMC_INTR_0_HARDWARE_MASK (~(1 << NV_PMC_INTR_0_SOFTWARE))
#define NV_PMC_INTR_0_SOFTWARE_MASK (1 << NV_PMC_INTR_0_SOFTWARE)
#define NV_PMC_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00000140)
#define NV_PMC_INTR_EN_0_INTA_DISABLED 0x00000000
#define NV_PMC_INTR_EN_0_INTA_HARDWARE 0x00000001
#define NV_PMC_INTR_EN_0_INTA_SOFTWARE 0x00000002
#define NV_PMC_ENABLE (NV2A_REGISTER_BASE + 0x00000200)
#define NV_PMC_ENABLE_PTIMER (1 << 16)
#define NV_PMC_ENABLE_PFB (1 << 20)
#define NV_PMC_ENABLE_PCRTC (1 << 24)


template<bool log>
void pmc::write(uint32_t addr, const uint32_t data)
{
	if constexpr (log) {
		log_io_write();
	}

	switch (addr)
	{
	case NV_PMC_BOOT_0:
		// This register is read-only
		break;

	case NV_PMC_BOOT_1: {
		// This register switches the endianness of all accesses done through BAR0 and BAR2/3 (when present)
		uint32_t mask = NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK;
		if (data & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK) {
			mask = NV_PMC_BOOT_1_ENDIAN0_BIG_MASK;
			nxbx_fatal("NV_PMC_BOOT_1: big endian switch not implemented");
			break;
		}
		endianness = ((data & NV_PMC_BOOT_1_ENDIAN24_BIG_MASK) | mask);
	}
	break;

	case NV_PMC_INTR_0:
		// Only NV_PMC_INTR_0_SOFTWARE is writable, the other bits are read-only
		int_status = (int_status & ~NV_PMC_INTR_0_SOFTWARE_MASK) | (data & NV_PMC_INTR_0_SOFTWARE_MASK);
		update_irq();
		break;

	case NV_PMC_INTR_EN_0:
		int_enabled = data;
		update_irq();
		break;

	case NV_PMC_ENABLE: {
		bool has_int_state_changed = false;
		engine_enabled = data;
		if ((data & NV_PMC_ENABLE_PTIMER) == 0) {
			m_machine->get<ptimer>().reset();
			has_int_state_changed = true;
		}
		if ((data & NV_PMC_ENABLE_PFB) == 0) {
			m_machine->get<pfb>().reset();
		}
		if ((data & NV_PMC_ENABLE_PCRTC) == 0) {
			m_machine->get<pcrtc>().reset();
			has_int_state_changed = true;
		}
		if (has_int_state_changed) {
			update_irq();
		}
	}
	break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

template<bool log>
uint32_t pmc::read(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PMC_BOOT_0:
		// Returns the id of the gpu
		value = NV_PMC_BOOT_0_ID_NV2A_A3_DEVID0; // value dumped from a Retail 1.0 xbox
		break;

	case NV_PMC_BOOT_1:
		// Returns the current endianness used for mmio accesses to the gpu
		value = endianness;
		break;

	case NV_PMC_INTR_0:
		value = int_status;
		break;

	case NV_PMC_INTR_EN_0:
		value = int_enabled;
		break;

	case NV_PMC_ENABLE:
		value = engine_enabled;
		break;

	default:
		nxbx_fatal("Unhandled %s read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		log_io_read();
	}

	return value;
}

void
pmc::update_irq()
{
	// Check for pending PCRTC interrupts
	if (m_machine->get<pcrtc>().int_status & m_machine->get<pcrtc>().int_enabled) {
		int_status |= (1 << NV_PMC_INTR_0_PCRTC);
	}
	else {
		int_status &= ~(1 << NV_PMC_INTR_0_PCRTC);
	}

	// Check for pending PTIMER interrupts
	if (m_machine->get<ptimer>().int_status & m_machine->get<ptimer>().int_enabled) {
		int_status |= (1 << NV_PMC_INTR_0_PTIMER);
	}
	else {
		int_status &= ~(1 << NV_PMC_INTR_0_PTIMER);
	}

	switch (int_enabled)
	{
	default:
	case NV_PMC_INTR_EN_0_INTA_DISABLED:
		// Don't do anything
		break;

	case NV_PMC_INTR_EN_0_INTA_HARDWARE:
		if (int_status & NV_PMC_INTR_0_HARDWARE_MASK) {
			m_machine->get<pic>().raise_irq(NV2A_IRQ_NUM);
		}
		else {
			m_machine->get<pic>().lower_irq(NV2A_IRQ_NUM);
		}
		break;

	case NV_PMC_INTR_EN_0_INTA_SOFTWARE:
		if (int_status & NV_PMC_INTR_0_SOFTWARE_MASK) {
			m_machine->get<pic>().raise_irq(NV2A_IRQ_NUM);
		}
		else {
			m_machine->get<pic>().lower_irq(NV2A_IRQ_NUM);
		}
		break;
	}
}

bool
pmc::update_io(bool is_update)
{
	bool log = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PMC_BASE, NV_PMC_SIZE, false,
		{
			.fnr32 = log ? cpu_read<pmc, uint32_t, &pmc::read<true>> : cpu_read<pmc, uint32_t, &pmc::read<false>>,
			.fnw32 = log ? cpu_write<pmc, uint32_t, &pmc::write<true>> : cpu_write<pmc, uint32_t, &pmc::write<false>>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pmc::reset()
{
	endianness = NV_PMC_BOOT_1_ENDIAN0_LITTLE_MASK | NV_PMC_BOOT_1_ENDIAN24_LITTLE_MASK;
	int_status = NV_PMC_INTR_0_NOT_PENDING;
	int_enabled = NV_PMC_INTR_EN_0_INTA_DISABLED;
	engine_enabled = 0;
}

bool
pmc::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
