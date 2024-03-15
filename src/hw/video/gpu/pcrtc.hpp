// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>

#define NV_PCRTC_START (NV2A_REGISTER_BASE + 0x00600800)


class machine;
class pmc;

class pcrtc {
public:
	pcrtc(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	uint32_t read(uint32_t addr);
	void write(uint32_t addr, const uint32_t data);
	uint32_t read_logger(uint32_t addr);
	void write_logger(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);

	friend class pmc;
	machine *const m_machine;
	struct {
		// Pending vblank interrupt. Writing a 0 has no effect, and writing a 1 clears the interrupt
		uint32_t int_status;
		// Enable/disable vblank interrupt
		uint32_t int_enabled;
		// The address of the framebuffer
		uint32_t fb_addr;
		// Unknown
		uint32_t unknown[1];
	};
};
