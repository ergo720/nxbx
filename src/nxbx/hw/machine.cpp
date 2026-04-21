// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#include "lib86cpu.hpp"
#include "host.hpp"
#include "machine.hpp"
#include "cpu.hpp"
#include "pic.hpp"
#include "pit.hpp"
#include "cmos.hpp"
#include "pci.hpp"
#include "smbus.hpp"
#include "eeprom.hpp"
#include "smc.hpp"
#include "adm1032.hpp"
#include "usb/ohci.hpp"
#include "video/conexant.hpp"
#include "video/vga.hpp"
#include "video/gpu/nv2a.hpp"


class machine::Impl
{
public:
	bool init(const boot_params &params, machine *machine);
	void deinit();
	void start();
	void exit();
	cpu *getCpu();
	pit *getPit();
	pic *getPic(uint32_t N);
	pci *getPci();
	cmos *getCmos();
	vga *getVga();
	smbus *getSmbus();
	eeprom *getEeprom();
	smc *getSmc();
	adm1032 *getAdm1032();
	conexant *getVideoEncoder();
	usb0 *getUsb(uint32_t N);
	nv2a *getGpu();
	cpu_t *get86cpu();
	void raise_irq(uint8_t a);
	void lower_irq(uint8_t a);
	void updateIoLogging();

private:
	std::unique_ptr<cpu> m_cpu;
	std::unique_ptr<pit> m_pit;
	std::unique_ptr<pic> m_pic[2]; // 0: master, 1: slave
	std::unique_ptr<pci> m_pci;
	std::unique_ptr<cmos> m_cmos;
	std::unique_ptr<nv2a> m_nv2a;
	std::unique_ptr<vga> m_vga;
	std::unique_ptr<smbus> m_smbus;
	std::unique_ptr<eeprom> m_eeprom;
	std::unique_ptr<smc> m_smc;
	std::unique_ptr<adm1032> m_adm1032;
	std::unique_ptr<conexant> m_conexant;
	std::unique_ptr<usb0> m_usb0;
};

bool machine::Impl::init(const boot_params &params, machine *machine)
{
	m_cpu = std::make_unique<cpu>();
	m_pit = std::make_unique<pit>();
	m_pic[0] = std::make_unique<pic>();
	m_pic[1] = std::make_unique<pic>();
	m_pci = std::make_unique<pci>();
	m_cmos = std::make_unique<cmos>();
	m_nv2a = std::make_unique<nv2a>();
	m_vga = std::make_unique<vga>();
	m_smbus = std::make_unique<smbus>();
	m_eeprom = std::make_unique<eeprom>();
	m_smc = std::make_unique<smc>();
	m_adm1032 = std::make_unique<adm1032>();
	m_conexant = std::make_unique<conexant>();
	m_usb0 = std::make_unique<usb0>();

	try {
		m_nv2a->allocEngines();
		m_cpu->init(params, machine);
		m_pic[0]->init(machine, 0);
		m_pic[1]->init(machine, 1);
		m_pit->init(machine);
		m_cmos->init(machine);
		m_pci->init(machine);
		m_nv2a->init(machine);
		m_vga->init(m_cpu.get(), m_nv2a.get());
		m_smbus->init(machine);
		m_eeprom->init(machine, log_module::eeprom);
		m_smc->init(machine, log_module::smc);
		m_adm1032->init(machine, log_module::adm1032);
		m_conexant->init(machine, log_module::conexant);
		m_usb0->init(machine);
	}
	catch (std::runtime_error e) {
		logger(e.what());
		return false;
	}

	return true;
}

void machine::Impl::deinit()
{
	m_cpu->deinit();
	m_cmos->deinit();
	m_smbus->deinit();
	m_nv2a->deinit();
}

void machine::Impl::start()
{
	m_cpu->start();
}

void machine::Impl::exit()
{
	m_cpu->exit();
}

void machine::Impl::raise_irq(uint8_t a)
{
	m_pic[a > 7 ? 1 : 0]->raiseIrq(a & 7);
}

void machine::Impl::lower_irq(uint8_t a)
{
	m_pic[a > 7 ? 1 : 0]->lowerIrq(a & 7);
}

void machine::Impl::updateIoLogging()
{
	try {
		m_cpu->updateIoLogging();
		m_pit->updateIoLogging();
		m_pic[0]->updateIoLogging();
		m_pic[1]->updateIoLogging();
		m_pci->updateIoLogging();
		m_cmos->updateIoLogging();
		m_nv2a->updateIoLogging();
		m_smbus->updateIoLogging();
		m_usb0->updateIoLogging();
	}
	catch (std::runtime_error e) {
		logger_mod_en(error, nxbx, "Failed to update logging settings of mmio handlers");
	}
	mem_init_region_io(m_cpu->get86cpu(), 0, 0, true, {}, m_cpu->get86cpu(), true, 3); // trigger the update in lib86cpu too
}

cpu_t *machine::Impl::get86cpu() { return m_cpu->get86cpu(); }
cpu *machine::Impl::getCpu() { return m_cpu.get(); }
pit *machine::Impl::getPit() { return m_pit.get(); }
pic *machine::Impl::getPic(uint32_t N) { return N < 2 ? m_pic[N].get() : nullptr; }
pci *machine::Impl::getPci() { return m_pci.get(); }
cmos *machine::Impl::getCmos() { return m_cmos.get(); }
vga *machine::Impl::getVga() { return m_vga.get(); }
smbus *machine::Impl::getSmbus() { return m_smbus.get(); }
eeprom *machine::Impl::getEeprom() { return m_eeprom.get(); }
smc *machine::Impl::getSmc() { return m_smc.get(); }
adm1032 *machine::Impl::getAdm1032() { return m_adm1032.get(); }
conexant *machine::Impl::getVideoEncoder() { return m_conexant.get(); }
usb0 *machine::Impl::getUsb(uint32_t N) { return m_usb0.get(); }
nv2a *machine::Impl::getGpu() { return m_nv2a.get(); }

/** Public interface implementation **/
bool machine::init(const boot_params &params)
{
	return m_impl->init(params, this);
}

void machine::deinit()
{
	m_impl->deinit();
}

void machine::start()
{
	m_impl->start();
}

void machine::exit()
{
	m_impl->exit();
}

void machine::updateIoLogging()
{
	m_impl->updateIoLogging();
}

void machine::raise_irq(uint8_t a)
{
	m_impl->raise_irq(a);
}

void machine::lower_irq(uint8_t a)
{
	m_impl->lower_irq(a);
}

cpu_t *machine::get86cpu() { return m_impl->get86cpu(); }
cpu *machine::getCpu() { return m_impl->getCpu(); }
pit *machine::getPit() { return m_impl->getPit(); }
pic *machine::getPic(uint32_t N) { return m_impl->getPic(N); }
pci *machine::getPci() { return m_impl->getPci(); }
cmos *machine::getCmos() { return m_impl->getCmos(); }
vga *machine::getVga() { return m_impl->getVga(); }
smbus *machine::getSmbus() { return m_impl->getSmbus(); }
eeprom *machine::getEeprom() { return m_impl->getEeprom(); }
smc *machine::getSmc() { return m_impl->getSmc(); }
adm1032 *machine::getAdm1032() { return m_impl->getAdm1032(); }
conexant *machine::getVideoEncoder() { return m_impl->getVideoEncoder(); }
usb0 *machine::getUsb(uint32_t N) { return m_impl->getUsb(N); }
nv2a *machine::getGpu() { return m_impl->getGpu(); }

machine::machine() : m_impl{std::make_unique<machine::Impl>()} {}
machine::~machine() {}
