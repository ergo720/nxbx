// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "lib86cpu.hpp"
#include "pbus.hpp"
#include "pfb.hpp"
#include "pmc.hpp"
#include "pcrtc.hpp"
#include "ptimer.hpp"
#include "pramin.hpp"
#include "pramdac.hpp"
#include "pfifo.hpp"
#include "pvideo.hpp"
#include "puser.hpp"
#include "pgraph.hpp"
// Must be included last because of the template functions nv2a_read/write, which require a complete definition for the engine objects
#include "nv2a.hpp"
#include "machine.hpp"
#include <cinttypes>

#define MODULE_NAME pmc


/** Private device implementation **/
class pmc::Impl
{
public:
	void init(cpu *cpu, nv2a *gpu, machine *machine);
	void reset();
	void updateIo() { updateIo(true); }
	void updateIrq();
	template<bool log>
	uint32_t read32(uint32_t addr);
	template<bool log>
	void write32(uint32_t addr, const uint32_t value);

private:
	void updateIo(bool is_update);
	template<bool is_write>
	auto getIoFunc(bool log, bool is_be);

	// connected devices
	pbus *m_pbus;
	pfb *m_pfb;
	pcrtc *m_pcrtc;
	ptimer *m_ptimer;
	pramin *m_pramin;
	pramdac *m_pramdac;
	pfifo *m_pfifo;
	pvideo *m_pvideo;
	puser *m_puser;
	pgraph *m_pgraph;
	cpu_t *m_lc86cpu;
	machine *m_machine;
	// atomic registers
	std::atomic_uint32_t m_int_status;
	std::atomic_uint32_t m_int_enabled;
	std::atomic_uint32_t m_endianness;
	std::atomic_uint32_t m_engine_enabled;
	// registers
	const std::unordered_map<uint32_t, const std::string> m_regs_info = {
		{ NV_PMC_BOOT_0, "NV_PMC_BOOT_0" },
		{ NV_PMC_BOOT_1, "NV_PMC_BOOT_1" },
		{ NV_PMC_INTR_0, "NV_PMC_INTR_0" },
		{ NV_PMC_INTR_EN_0, "NV_PMC_INTR_EN_0" },
		{ NV_PMC_ENABLE, "NV_PMC_ENABLE" }
	};
};

template<bool log>
void pmc::Impl::write32(uint32_t addr, const uint32_t value)
{
	if constexpr (log) {
		nv2a_log_write();
	}

	switch (addr)
	{
	case NV_PMC_BOOT_0:
		// This register is read-only
		break;

	case NV_PMC_BOOT_1: {
		uint32_t old_state = m_endianness;
		uint32_t new_endianness = (value ^ NV_PMC_BOOT_1_ENDIAN24_BIG) & NV_PMC_BOOT_1_ENDIAN24_BIG;
		m_endianness = (new_endianness | (new_endianness >> 24));
		if ((old_state ^ m_endianness) & NV_PMC_BOOT_1_ENDIAN24_BIG) {
			try {
				this->updateIo();
				m_pbus->updateIo();
				m_puser->updateIo();
				m_pramdac->updateIo();
				m_pramin->updateIo();
				m_pfifo->updateIo();
				m_ptimer->updateIo();
				m_pfb->updateIo();
				m_pcrtc->updateIo();
				m_pvideo->updateIo();
				m_pgraph->updateIo();
			}
			catch (std::runtime_error e) {
				nxbx_msg_fatal(e.what());
				break;
			}
			mem_init_region_io(m_lc86cpu, 0, 0, true, {}, m_lc86cpu, true, 3); // trigger the update in lib86cpu too
		}
	}
	break;

	case NV_PMC_INTR_0:
		// Only NV_PMC_INTR_0_SOFTWARE is writable, the other bits are read-only
		m_int_status = (m_int_status & ~(1 << NV_PMC_INTR_0_SOFTWARE)) | (value & (1 << NV_PMC_INTR_0_SOFTWARE));
		updateIrq();
		break;

	case NV_PMC_INTR_EN_0:
		m_int_enabled = value;
		updateIrq();
		break;

	case NV_PMC_ENABLE: {
		bool has_int_state_changed = false;
		uint32_t old_state = m_engine_enabled;
		m_engine_enabled = value;
		if ((value & NV_PMC_ENABLE_PFIFO) == 0) {
			m_pfifo->reset();
			has_int_state_changed = true;
		}
		if ((value & NV_PMC_ENABLE_PGRAPH) == 0) {
			m_pgraph->reset();
			has_int_state_changed = true;
		}
		if ((value & NV_PMC_ENABLE_PTIMER) == 0) {
			m_ptimer->reset();
			has_int_state_changed = true;
		}
		if ((value & NV_PMC_ENABLE_PFB) == 0) {
			m_pfb->reset();
		}
		if ((value & NV_PMC_ENABLE_PCRTC) == 0) {
			m_pcrtc->reset();
			has_int_state_changed = true;
		}
		if ((value & NV_PMC_ENABLE_PVIDEO) == 0) {
			m_pvideo->reset();
		}
		if ((old_state ^ m_engine_enabled) & NV_PMC_ENABLE_ALL) {
			try {
				m_pfifo->updateIo();
				m_pgraph->updateIo();
				m_ptimer->updateIo();
				m_pfb->updateIo();
				m_pcrtc->updateIo();
				m_pvideo->updateIo();
			}
			catch (std::runtime_error e) {
				nxbx_msg_fatal(e.what());
				break;
			}
			mem_init_region_io(m_lc86cpu, 0, 0, true, {}, m_lc86cpu, true, 3); // trigger the update in lib86cpu too
		}
		if (has_int_state_changed) {
			updateIrq();
		}
	}
	break;

	default:
		nxbx_fatal("Unhandled write at address 0x%" PRIX32 " with value 0x%" PRIX32, addr, value);
	}
}

template<bool log>
uint32_t pmc::Impl::read32(uint32_t addr)
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
		value = m_endianness;
		break;

	case NV_PMC_INTR_0:
		value = m_int_status;
		break;

	case NV_PMC_INTR_EN_0:
		value = m_int_enabled;
		break;

	case NV_PMC_ENABLE:
		value = m_engine_enabled;
		break;

	default:
		nxbx_fatal("Unhandled %s read at address 0x%" PRIX32, addr);
	}

	if constexpr (log) {
		nv2a_log_read();
	}

	return value;
}

void pmc::Impl::updateIrq()
{
	// Check for pending PCRTC interrupts
	if (m_pcrtc->read32(NV_PCRTC_INTR_0) & m_pcrtc->read32(NV_PCRTC_INTR_EN_0)) {
		m_int_status |= (1 << NV_PMC_INTR_0_PCRTC);
	}
	else {
		m_int_status &= ~(1 << NV_PMC_INTR_0_PCRTC);
	}

	// Check for pending PTIMER interrupts
	if (m_ptimer->read32(NV_PTIMER_INTR_0) & m_ptimer->read32(NV_PTIMER_INTR_EN_0)) {
		m_int_status |= (1 << NV_PMC_INTR_0_PTIMER);
	}
	else {
		m_int_status &= ~(1 << NV_PMC_INTR_0_PTIMER);
	}

	// Check for pending PFIFO interrupts
	if (m_pfifo->read32(NV_PFIFO_INTR_0) & m_pfifo->read32(NV_PFIFO_INTR_EN_0)) {
		m_int_status |= (1 << NV_PMC_INTR_0_PFIFO);
	}
	else {
		m_int_status &= ~(1 << NV_PMC_INTR_0_PFIFO);
	}

	// Check for pending PGRAPH interrupts
	if (m_pgraph->read32(NV_PGRAPH_INTR) & m_pgraph->read32(NV_PGRAPH_INTR_EN)) {
		m_int_status |= (1 << NV_PMC_INTR_0_PGRAPH);
	}
	else {
		m_int_status &= ~(1 << NV_PMC_INTR_0_PGRAPH);
	}

	// Check for pending PVIDEO interrupts
	if (m_pvideo->read32(NV_PVIDEO_INTR) & m_pvideo->read32(NV_PVIDEO_INTR_EN)) {
		m_int_status |= (1 << NV_PMC_INTR_0_PVIDEO);
	}
	else {
		m_int_status &= ~(1 << NV_PMC_INTR_0_PVIDEO);
	}

	switch (m_int_enabled)
	{
	default:
	case NV_PMC_INTR_EN_0_INTA_DISABLED:
		m_machine->lower_irq(NV2A_IRQ_NUM);
		break;

	case NV_PMC_INTR_EN_0_INTA_HARDWARE:
		if (m_int_status & ~(1 << NV_PMC_INTR_0_SOFTWARE)) {
			m_machine->raise_irq(NV2A_IRQ_NUM);
		}
		else {
			m_machine->lower_irq(NV2A_IRQ_NUM);
		}
		break;

	case NV_PMC_INTR_EN_0_INTA_SOFTWARE:
		if (m_int_status & (1 << NV_PMC_INTR_0_SOFTWARE)) {
			m_machine->raise_irq(NV2A_IRQ_NUM);
		}
		else {
			m_machine->lower_irq(NV2A_IRQ_NUM);
		}
		break;
	}
}

template<bool is_write>
auto pmc::Impl::getIoFunc(bool log, bool is_be)
{
	if constexpr (is_write) {
		if (log) {
			return is_be ? nv2a_write<pmc::Impl, uint32_t, &pmc::Impl::write32<true>, big> : nv2a_write<pmc::Impl, uint32_t, &pmc::Impl::write32<true>, le>;
		}
		else {
			return is_be ? nv2a_write<pmc::Impl, uint32_t, &pmc::Impl::write32<false>, big> : nv2a_write<pmc::Impl, uint32_t, &pmc::Impl::write32<false>, le>;
		}
	}
	else {
		if (log) {
			return is_be ? nv2a_read<pmc::Impl, uint32_t, &pmc::Impl::read32<true>, big> : nv2a_read<pmc::Impl, uint32_t, &pmc::Impl::read32<true>, le>;
		}
		else {
			return is_be ? nv2a_read<pmc::Impl, uint32_t, &pmc::Impl::read32<false>, big> : nv2a_read<pmc::Impl, uint32_t, &pmc::Impl::read32<false>, le>;
		}
	}
}

void pmc::Impl::updateIo(bool is_update)
{
	bool log = module_enabled();
	bool is_be = m_endianness & NV_PMC_BOOT_1_ENDIAN24_BIG;
	if (!LC86_SUCCESS(mem_init_region_io(m_lc86cpu, NV_PMC_BASE, NV_PMC_SIZE, false,
		{
			.fnr32 = getIoFunc<false>(log, is_be),
			.fnw32 = getIoFunc<true>(log, is_be)
		},
		this, is_update, is_update))) {
		throw std::runtime_error(lv2str(highest, "Failed to update mmio region"));
	}
}

void pmc::Impl::reset()
{
	// Values dumped from a Retail 1.0 xbox
	m_endianness = NV_PMC_BOOT_1_ENDIAN0_LITTLE | NV_PMC_BOOT_1_ENDIAN24_LITTLE;
	m_int_status = NV_PMC_INTR_0_NOT_PENDING;
	m_int_enabled = NV_PMC_INTR_EN_0_INTA_DISABLED;
	m_engine_enabled = NV_PMC_ENABLE_PTIMER | NV_PMC_ENABLE_PFB | NV_PMC_ENABLE_PCRTC;
}

void pmc::Impl::init(cpu *cpu, nv2a *gpu, machine *machine)
{
	m_pbus = gpu->getPbus();
	m_pfb = gpu->getPfb();
	m_pcrtc = gpu->getPcrtc();
	m_ptimer = gpu->getPtimer();
	m_pramin = gpu->getPramin();
	m_pramdac = gpu->getPramdac();
	m_pfifo = gpu->getPfifo();
	m_pvideo = gpu->getPvideo();
	m_puser = gpu->getPuser();
	m_pgraph = gpu->getPgraph();
	m_lc86cpu = cpu->get86cpu();
	m_machine = machine;
	reset();
	updateIo(false);
}

/** Public interface implementation **/
void pmc::init(cpu *cpu, nv2a *gpu, machine *machine)
{
	m_impl->init(cpu, gpu, machine);
}

void pmc::reset()
{
	m_impl->reset();
}

void pmc::updateIo()
{
	m_impl->updateIo();
}

void pmc::updateIrq()
{
	m_impl->updateIrq();
}

uint32_t pmc::read32(uint32_t addr)
{
	return m_impl->read32<false>(addr);
}

void pmc::write32(uint32_t addr, const uint32_t value)
{
	m_impl->write32<false>(addr, value);
}

pmc::pmc() : m_impl{std::make_unique<pmc::Impl>()} {}
pmc::~pmc() {}
