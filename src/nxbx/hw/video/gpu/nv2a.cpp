// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#include "pmc.hpp"
#include "pramdac.hpp"
#include "pbus.hpp"
#include "pfb.hpp"
#include "pcrtc.hpp"
#include "ptimer.hpp"
#include "pfifo.hpp"
#include "pramin.hpp"
#include "pvga.hpp"
#include "pvideo.hpp"
#include "puser.hpp"
#include "pgraph.hpp"
#include "nv2a.hpp"
#include "machine.hpp"


/** Private device implementation **/
class nv2a::Impl
{
public:
	bool init(nv2a *gpu, machine *machine);
	void deinit();
	uint64_t getNextUpdateTime(uint64_t now);
	pmc *getPmc();
	pcrtc *getPcrtc();
	pramdac *getPramdac();
	ptimer *getPtimer();
	pfb *getPfb();
	pbus *getPbus();
	pramin *getPramin();
	puser *getPuser();
	pfifo *getPfifo();
	pvga *getPvga();
	pvideo *getPvideo();
	pgraph *getPgraph();
	void updateIoLogging();
	DmaObj getDmaObj(uint32_t addr);

private:
	std::unique_ptr<pmc> m_pmc;
	std::unique_ptr<pcrtc> m_pcrtc;
	std::unique_ptr<pramdac> m_pramdac;
	std::unique_ptr<ptimer> m_ptimer;
	std::unique_ptr<pfb> m_pfb;
	std::unique_ptr<pbus> m_pbus;
	std::unique_ptr<pramin> m_pramin;
	std::unique_ptr<pfifo> m_pfifo;
	std::unique_ptr<pvga> m_pvga;
	std::unique_ptr<pvideo> m_pvideo;
	std::unique_ptr<puser> m_puser;
	std::unique_ptr<pgraph> m_pgraph;
};

bool nv2a::Impl::init(nv2a *gpu, machine *machine)
{
	m_pmc = std::make_unique<pmc>();
	m_pramdac = std::make_unique<pramdac>();
	m_pbus = std::make_unique<pbus>();
	m_pfb = std::make_unique<pfb>();
	m_pcrtc = std::make_unique<pcrtc>();
	m_ptimer = std::make_unique<ptimer>();
	m_pramin = std::make_unique<pramin>();
	m_pfifo = std::make_unique<pfifo>();
	m_pvga = std::make_unique<pvga>();
	m_pvideo = std::make_unique<pvideo>();
	m_puser = std::make_unique<puser>();
	m_pgraph = std::make_unique<pgraph>();

	cpu *cpu = machine->getCpu();

	if (!m_pmc->init(cpu, gpu, machine)) {
		return false;
	}
	if (!m_pramdac->init(cpu, gpu)) {
		return false;
	}
	if (!m_pbus->init(cpu, gpu, machine->getPci())) {
		return false;
	}
	if (!m_pfb->init(cpu, gpu)) {
		return false;
	}
	if (!m_pcrtc->init(cpu, gpu)) {
		return false;
	}
	if (!m_ptimer->init(cpu, gpu)) {
		return false;
	}
	if (!m_pramin->init(cpu, gpu)) {
		return false;
	}
	if (!m_pfifo->init(cpu, gpu)) {
		return false;
	}
	if (!m_pvga->init(cpu, gpu, machine->getVga())) {
		return false;
	}
	if (!m_pvideo->init(cpu, gpu)) {
		return false;
	}
	if (!m_puser->init(cpu, gpu)) {
		return false;
	}
	if (!m_pgraph->init(cpu, gpu)) {
		return false;
	}
	return true;
}

void nv2a::Impl::deinit()
{
	if (m_pfifo) {
		m_pfifo->deinit();
	}
}

uint64_t nv2a::Impl::getNextUpdateTime(uint64_t now)
{
	return m_ptimer->getNextAlarmTime(now);
}

pmc *nv2a::Impl::getPmc() { return m_pmc.get(); }
pcrtc *nv2a::Impl::getPcrtc() { return m_pcrtc.get(); }
pramdac *nv2a::Impl::getPramdac() { return m_pramdac.get(); }
ptimer *nv2a::Impl::getPtimer() { return m_ptimer.get(); }
pfb *nv2a::Impl::getPfb() { return m_pfb.get(); }
pbus *nv2a::Impl::getPbus() { return m_pbus.get(); }
pramin *nv2a::Impl::getPramin() { return m_pramin.get(); }
puser *nv2a::Impl::getPuser() { return m_puser.get(); }
pfifo *nv2a::Impl::getPfifo() { return m_pfifo.get(); }
pvga *nv2a::Impl::getPvga() { return m_pvga.get(); }
pvideo *nv2a::Impl::getPvideo() { return m_pvideo.get(); }
pgraph *nv2a::Impl::getPgraph() { return m_pgraph.get(); }

DmaObj nv2a::Impl::getDmaObj(uint32_t addr)
{
	/*
	A dma object has the following memory layout:
	base+0: flags -> 0:11 class type, 12:13 page table stuff, 16:17 mem type, 20:31 high 12 bits of target addr
	base+4: limit -> 0:31 addr limit for the resource at the target addr
	base+8: addr -> 12:31 low 20 bits of target addr
	*/

	// TODO: this should also consider the endianness bit of NV_PFIFO_CACHE1_DMA_FETCH
	uint32_t flags = m_pramin->read32(NV_PRAMIN_BASE + addr);
	uint32_t limit = m_pramin->read32(NV_PRAMIN_BASE + addr + 4);
	uint32_t addr_info = m_pramin->read32(NV_PRAMIN_BASE + addr + 8);

	return DmaObj{
		.class_type = flags & NV_DMA_CLASS,
		.mem_type = (flags & NV_DMA_TARGET) >> 16,
		.target_addr = (((flags & NV_DMA_ADJUST) >> 20) | (addr_info & NV_DMA_ADDRESS)) & (RAM_SIZE128 - 1),
		.limit = limit,
	};
}

void nv2a::Impl::updateIoLogging()
{
	m_pmc->updateIo();
	m_pcrtc->updateIo();
	m_pramdac->updateIo();
	m_ptimer->updateIo();
	m_pfb->updateIo();
	m_pbus->updateIo();
	m_pramin->updateIo();
	m_pfifo->updateIo();
	m_pvga->updateIo();
	m_pvideo->updateIo();
	m_puser->updateIo();
	m_pgraph->updateIo();
}

/** Public interface implementation **/
bool nv2a::init(machine *machine)
{
	return m_impl->init(this, machine);
}

void nv2a::deinit()
{
	m_impl->deinit();
}

DmaObj nv2a::getDmaObj(uint32_t addr)
{
	return m_impl->getDmaObj(addr);
}

void nv2a::updateIoLogging()
{
	m_impl->updateIoLogging();
}

uint64_t nv2a::getNextUpdateTime(uint64_t now)
{
	return m_impl->getNextUpdateTime(now);
}

pmc *nv2a::getPmc() { return m_impl->getPmc(); }
pcrtc *nv2a::getPcrtc() { return m_impl->getPcrtc(); }
pramdac *nv2a::getPramdac() { return m_impl->getPramdac(); }
ptimer *nv2a::getPtimer() { return m_impl->getPtimer(); }
pfb *nv2a::getPfb() { return m_impl->getPfb(); }
pbus *nv2a::getPbus() { return m_impl->getPbus(); }
pramin *nv2a::getPramin() { return m_impl->getPramin(); }
puser *nv2a::getPuser() { return m_impl->getPuser(); }
pfifo *nv2a::getPfifo() { return m_impl->getPfifo(); }
pvga *nv2a::getPvga() { return m_impl->getPvga(); }
pvideo *nv2a::getPvideo() { return m_impl->getPvideo(); }
pgraph *nv2a::getPgraph() { return m_impl->getPgraph(); }

nv2a::nv2a() : m_impl{std::make_unique<nv2a::Impl>()} {}
nv2a::~nv2a() {}
