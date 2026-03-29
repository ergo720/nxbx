// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>
#include "nv2a_defs.hpp"

#define NV_PTIMER 0x00009000
#define NV_PTIMER_BASE (NV2A_REGISTER_BASE + NV_PTIMER)
#define NV_PTIMER_SIZE 0x1000

#define NV_PTIMER_INTR_0 (NV2A_REGISTER_BASE + 0x00009100) // Pending alarm interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
#define NV_PTIMER_INTR_0_ALARM_NOT_PENDING 0x00000000
#define NV_PTIMER_INTR_0_ALARM_PENDING 0x00000001
#define NV_PTIMER_INTR_EN_0 (NV2A_REGISTER_BASE + 0x00009140) // Enable/disable alarm interrupt
#define NV_PTIMER_INTR_EN_0_ALARM_DISABLED 0x00000000
#define NV_PTIMER_INTR_EN_0_ALARM_ENABLED 0x00000001
#define NV_PTIMER_NUMERATOR (NV2A_REGISTER_BASE + 0x00009200) // Divider forms a ratio which is then used to multiply the clock frequency
#define NV_PTIMER_NUMERATOR_MASK 0xFFFF
#define NV_PTIMER_DENOMINATOR (NV2A_REGISTER_BASE + 0x00009210) // Multiplier forms a ratio which is then used to multiply the clock frequency
#define NV_PTIMER_DENOMINATOR_MASK 0xFFFF
#define NV_PTIMER_TIME_0 (NV2A_REGISTER_BASE + 0x00009400) // Current gpu time (low bits)
#define NV_PTIMER_TIME_1 (NV2A_REGISTER_BASE + 0x00009410) // Current gpu time (high bits)
#define NV_PTIMER_ALARM_0 (NV2A_REGISTER_BASE + 0x00009420) // Counter value that triggers the alarm interrupt


class cpu;
class nv2a;

class ptimer
{
public:
	ptimer();
	~ptimer();
	bool init(cpu *cpu, nv2a *gpu);
	void reset();
	void updateIo();
	uint64_t getNextAlarmTime(uint64_t now);
	uint32_t read32(uint32_t addr);
	void write32(uint32_t addr, const uint32_t value);
	uint8_t isCounterOn();
	void setCounterPeriod(uint64_t new_period);
	uint64_t counterToUs();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
