// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>


class cpu;
class nv2a;

class vga
{
public:
	vga();
	~vga();
	void init(cpu *cpu, nv2a *gpu);
	void reset();
	uint8_t ioRead8(uint32_t addr);
	void ioWrite8(uint32_t addr, const uint8_t value);
	void ioWrite16(uint32_t addr, const uint16_t value);
	uint8_t memRead8(uint32_t addr);
	uint16_t memRead16(uint32_t addr);
	void memWrite8(uint32_t addr, const uint8_t value);
	void memWrite16(uint32_t addr, const uint16_t value);
	void update();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};