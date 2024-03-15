// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <stdint.h>
#include <vector>


class machine;

class vga {
public:
	vga(machine *machine) : m_machine(machine) {}
	bool init();
	void reset();
	uint8_t io_read8(uint32_t addr);
	void io_write8(uint32_t addr, const uint8_t data);
	void io_write16(uint32_t addr, const uint16_t data);
	uint8_t mem_read8(uint32_t addr);
	uint16_t mem_read16(uint32_t addr);
	void mem_write8(uint32_t addr, const uint8_t data);
	void mem_write16(uint32_t addr, const uint16_t data);
	void update();

private:
	void update_size();
	void change_renderer();
	void complete_redraw();
	void update_mem_access();
	void update_all_dac_entries();
	void update_one_dac_entry(int i);
	void change_attr_cache(int i);
	uint8_t alu_rotate(uint8_t value);

	machine *const m_machine;
	// CRT Controller
	uint8_t crt[256], crt_index;
	// Attribute Controller
	uint8_t attr[32], attr_index, attr_palette[16];
	// Sequencer
	uint8_t seq[8], seq_index;
	// Graphics Registers
	uint8_t gfx[256], gfx_index;
	// Digital To Analog
	uint8_t dac[1024];
	uint32_t dac_palette[256];
	uint8_t dac_mask,
		dac_state, // 0 if reading, 3 if writing
		dac_address, // Index into dac_palette
		dac_color, // Current color being read (0: red, 1: blue, 2: green)
		dac_read_address; // same as dac_address, but for reads

	// Status stuff
	uint8_t status[2];

	// Miscellaneous Graphics Register
	uint8_t misc;

	// Text Mode Rendering variables
	uint8_t char_width;
	uint32_t character_map[2];

	// General rendering variables
	uint8_t pixel_panning, current_pixel_panning;
	uint32_t total_height, total_width;
	int renderer;
	uint32_t current_scanline, character_scanline;
	uint32_t *framebuffer; // where pixel data is written to
	uint32_t framebuffer_offset; // the offset being written to right now
	uint32_t vram_addr; // Current VRAM offset being accessed by renderer
	uint32_t scanlines_to_update; // Number of scanlines to update per vga_update

	// Memory access settings
	uint8_t write_access, read_access, write_mode;
	uint32_t vram_window_base, vram_window_size;
	union {
		uint8_t latch8[4];
		uint32_t latch32;
	};

	uint32_t framectr;
	uint32_t vram_size;
	uint8_t *vram;

	std::vector<uint8_t> vbe_scanlines_modified;

	// Screen data cannot change if memory_modified is zero.
	int memory_modified;
};