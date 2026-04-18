// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PRMVGA 0x000A0000
#define NV_PRMVGA_BASE (NV2A_REGISTER_BASE + NV_PRMVGA)
#define NV_PRMVGA_SIZE 0x20000
#define NV_PRMVIO 0x000C0000
#define NV_PRMVIO_BASE (NV2A_REGISTER_BASE + NV_PRMVIO)
#define NV_PRMVIO_SIZE 0x8000
#define NV_PRMCIO 0x00601000
#define NV_PRMCIO_BASE (NV2A_REGISTER_BASE + NV_PRMCIO)
#define NV_PRMCIO_SIZE 0x1000
#define NV_PRMDIO 0x00681000
#define NV_PRMDIO_BASE (NV2A_REGISTER_BASE + NV_PRMDIO)
#define NV_PRMDIO_SIZE 0x1000


class cpu;
class nv2a;
class vga;

class pvga
{
public:
	pvga();
	~pvga();
	void init(cpu *cpu, nv2a *gpu, vga *vga);
	void reset();
	void updateIo();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
