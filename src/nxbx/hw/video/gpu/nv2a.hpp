// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "cpu.hpp"
#include <bit>
#include <memory>


struct DmaObj {
	uint32_t class_type;
	uint32_t mem_type;
	uint32_t target_addr;
	uint32_t limit;
};

enum engine_enabled : int {
	off = 0,
	on = 1
};
enum engine_endian : int {
	le = 0,
	big = 1
};

class pmc;
class pramdac;
class pbus;
class pfb;
class pcrtc;
class ptimer;
class pramin;
class pfifo;
class pvga;
class pvideo;
class puser;
class pgraph;

class nv2a
{
public:
	nv2a();
	~nv2a();
	bool init(machine *machine);
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
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

#define nv2a_log_read() log_read<log_module::MODULE_NAME, false, 3>(m_regs_info, addr, value);
#define nv2a_log_write() log_write<log_module::MODULE_NAME, false, 3>(m_regs_info, addr, value);

template<typename D, typename T, auto f, engine_endian is_be, uint32_t base = 0>
T nv2a_read(uint32_t addr, void *opaque)
{
	T value = cpu_read<D, T, f, base>(addr, opaque);
	if constexpr (is_be) {
		value = std::byteswap<T>(value);
	}
	return value;
}

template<typename D, typename T, auto f, engine_endian is_be, uint32_t base = 0>
void nv2a_write(uint32_t addr, T value, void *opaque)
{
	if constexpr (is_be) {
		value = std::byteswap<T>(value);
	}
	cpu_write<D, T, f, base>(addr, value, opaque);
}
