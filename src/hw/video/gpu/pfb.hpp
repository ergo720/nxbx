// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>


class machine;
class pramin;

class pfb {
public:
	pfb(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	void update_io_logging() { update_io(true); }
	uint32_t read(uint32_t addr);
	void write(uint32_t addr, const uint32_t data);
	uint32_t read_logger(uint32_t addr);
	void write_logger(uint32_t addr, const uint32_t data);

private:
	bool update_io(bool is_update);

	friend class pramin;
	machine *const m_machine;
	struct {
		// Appear to contain info about the ram modules
		uint32_t cfg0, cfg1;
		// FIXME: unknown what this does
		uint32_t nvm;
		// Returns the size of the framebuffer in MiB in the bits 20-31. Bit 0 is a flag that indicates > 4 GiB of fb when set
		uint32_t cstatus;
	};
};
