// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "../../../clock.hpp"
#include "machine.hpp"

#define MODULE_NAME pramdac

#define NV_PRAMDAC 0x00680300
#define NV_PRAMDAC_BASE (NV2A_REGISTER_BASE + NV_PRAMDAC)
#define NV_PRAMDAC_SIZE 0xD00

#define NV_PRAMDAC_NVPLL_COEFF (NV2A_REGISTER_BASE + 0x00680500)
#define NV_PRAMDAC_NVPLL_COEFF_MDIV_MASK 0x000000FF
#define NV_PRAMDAC_NVPLL_COEFF_NDIV_MASK 0x0000FF00
#define NV_PRAMDAC_NVPLL_COEFF_PDIV_MASK 0x00070000
#define NV_PRAMDAC_MPLL_COEFF (NV2A_REGISTER_BASE + 0x00680504)
#define NV_PRAMDAC_VPLL_COEFF (NV2A_REGISTER_BASE + 0x00680508)


void
pramdac::write32(uint32_t addr, const uint32_t data)
{
	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF: {
		// NOTE: if the m value is zero, then the final frequency is also zero
		nvpll_coeff = data;
		uint64_t m = data & NV_PRAMDAC_NVPLL_COEFF_MDIV_MASK;
		uint64_t n = (data & NV_PRAMDAC_NVPLL_COEFF_NDIV_MASK) >> 8;
		uint64_t p = (data & NV_PRAMDAC_NVPLL_COEFF_PDIV_MASK) >> 16;
		core_freq = m ? ((NV2A_CRYSTAL_FREQ * n) / (1ULL << p) / m) : 0;
		if (m_machine->get<ptimer>().counter_active) {
			m_machine->get<ptimer>().counter_period = m_machine->get<ptimer>().counter_to_us();
			cpu_set_timeout(m_machine->get<cpu_t *>(), m_machine->get<cpu>().check_periodic_events(timer::get_now()));
		}
	}
	break;

	case NV_PRAMDAC_MPLL_COEFF:
		mpll_coeff = data;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		vpll_coeff = data;
		break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, data);
	}
}

uint32_t
pramdac::read32(uint32_t addr)
{
	uint32_t value = 0;

	switch (addr)
	{
	case NV_PRAMDAC_NVPLL_COEFF:
		value = nvpll_coeff;
		break;

	case NV_PRAMDAC_MPLL_COEFF:
		value = mpll_coeff;
		break;

	case NV_PRAMDAC_VPLL_COEFF:
		value = vpll_coeff;
		break;

	default:
		nxbx_fatal("Unhandled %s read at address 0x%" PRIX32, addr);
	}

	return value;
}

uint8_t
pramdac::read8(uint32_t addr)
{
	// This handler is necessary because Direct3D_CreateDevice reads the n value by accessing the second byte of the register, even though the coefficient
	// registers are supposed to be four bytes instead. This is probably due to compiler optimizations

	uint32_t addr_base = addr & ~3;
	uint32_t addr_offset = (addr & 3) << 3;
	uint32_t value = read32(addr_base);
	return uint8_t((value & (0xFF << addr_offset)) >> addr_offset);
}

uint8_t
pramdac::read8_logger(uint32_t addr)
{
	uint8_t data = read8(addr);
	log_io_read();
	return data;
}

uint32_t
pramdac::read32_logger(uint32_t addr)
{
	uint32_t data = read32(addr);
	log_io_read();
	return data;
}

void
pramdac::write32_logger(uint32_t addr, const uint32_t data)
{
	log_io_write();
	write32(addr, data);
}

bool
pramdac::update_io(bool is_update)
{
	bool enable = module_enabled();
	if (!LC86_SUCCESS(mem_init_region_io(m_machine->get<cpu_t *>(), NV_PRAMDAC_BASE, NV_PRAMDAC_SIZE, false,
		{
			.fnr8 = enable ? cpu_read<pramdac, uint8_t, &pramdac::read8_logger> : cpu_read<pramdac, uint8_t, &pramdac::read8>,
			.fnr32 = enable ? cpu_read<pramdac, uint32_t, &pramdac::read32_logger> : cpu_read<pramdac, uint32_t, &pramdac::read32>,
			.fnw32 = enable ? cpu_write<pramdac, uint32_t, &pramdac::write32_logger> : cpu_write<pramdac, uint32_t, &pramdac::write32>
		},
		this, is_update, is_update))) {
		logger_en(error, "Failed to update mmio region");
		return false;
	}

	return true;
}

void
pramdac::reset()
{
	// Values dumped from a Retail 1.0 xbox
	core_freq = NV2A_CLOCK_FREQ;
	nvpll_coeff = 0x00011C01;
	mpll_coeff = 0x00007702;
	vpll_coeff = 0x0003C20D;
}

bool
pramdac::init()
{
	if (!update_io(false)) {
		return false;
	}

	reset();
	return true;
}
