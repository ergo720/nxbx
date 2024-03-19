// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

// This code is derived from https://github.com/ergo720/halfix/blob/master/src/hardware/vga.cpp

#include "machine.hpp"
#include <cstring>

#define MODULE_NAME vga

#define MASK(n) (uint8_t)(~n)
#define DO_MASK(n) xor_ ^= mask_enabled& n ? value& lut32[n] : mask& lut32[n]


enum {
	CHAIN4,
	ODDEVEN,
	NORMAL,
	READMODE_1
};

enum {
	BLANK_RENDERER = 0, // Shows nothing on the screen
	ALPHANUMERIC_RENDERER = 2, // AlphaNumeric Mode (aka text mode)
	MODE_13H_RENDERER = 4, // Mode 13h
	RENDER_4BPP = 6,
	// VBE render modes
	RENDER_32BPP = 8, // Windows XP uses this
	RENDER_8BPP = 10, // Debian uses this one
	RENDER_16BPP = 12,
	RENDER_24BPP = 14
};


static void
display_set_resolution(uint32_t width, uint32_t height)
{
	// TODO: this should update the size of the rendering window
}

static void *
display_get_pixels()
{
	// TODO: this should get the rendering target used by the rendering window
	return nullptr;
}

static void
display_update()
{
	// TODO: this should redraw the image shown in the rendering window
}

static void
expand32_alt(uint8_t *ptr, int v4)
{
	ptr[0] = v4 & 1 ? 0xFF : 0;
	ptr[1] = v4 & 2 ? 0xFF : 0;
	ptr[2] = v4 & 4 ? 0xFF : 0;
	ptr[3] = v4 & 8 ? 0xFF : 0;
}

static uint32_t
expand32(int v4)
{
	uint32_t r = v4 & 1 ? 0xFF : 0;
	r |= v4 & 2 ? 0xFF00 : 0;
	r |= v4 & 4 ? 0xFF0000 : 0;
	r |= v4 & 8 ? 0xFF000000 : 0;
	return r;
}

static uint8_t
c6to8(uint8_t a)
{
	uint8_t b = a & 1;
	return a << 2 | b << 1 | b;
}

static uint32_t
b8to32(uint8_t x)
{
	uint32_t y = x;
	y |= y << 8;
	return y | (y << 16);
}

// If a bit in "mask_enabled" is set, then replace value with the data in "mask," otherwise keep the same
// Example: value=0x12345678 mask=0x9ABCDEF0 mask_enabled=0b1010 result=0x9A34DE78
static inline uint32_t
do_mask(uint32_t value, uint32_t mask, int mask_enabled)
{
	static uint32_t lut32[9] = { 0, 0xFF, 0xFF00, 0, 0xFF0000, 0, 0, 0, 0xFF000000 };
	// Uses XOR for fast bit replacement
	uint32_t xor_ = value ^ mask;
	DO_MASK(1);
	DO_MASK(2);
	DO_MASK(4);
	DO_MASK(8);
	return xor_;
}

static uint8_t
bpp4_to_offset(uint8_t i, uint8_t j, uint8_t k)
{
	return ((i & (0x80 >> j)) != 0) ? 1 << k : 0;
}

static uint32_t
char_map_address(int b)
{
	return b << 13;
}

uint8_t
vga::alu_rotate(uint8_t value)
{
	uint8_t rotate_count = gfx[3] & 7;
	return ((value >> rotate_count) | (value << (8 - rotate_count))) & 0xFF;
}

void
vga::update_one_dac_entry(int i)
{
	int index = i << 2;
	dac_palette[i] = 255 << 24 | c6to8(dac[index | 0]) << 16 | c6to8(dac[index | 1]) << 8 | c6to8(dac[index | 2]);
}

void
vga::update_all_dac_entries()
{
	for (int i = 0; i < 256; i++) {
		update_one_dac_entry(i);
	}
}

void
vga::change_attr_cache(int i)
{
	if (attr[0x10] & 0x80) {
		attr_palette[i] = (attr[i] & 0x0F) | ((attr[0x14] << 4) & 0xF0);
	}
	else {
		attr_palette[i] = (attr[i] & 0x3F) | ((attr[0x14] << 4) & 0xC0);
	}
}

void
vga::update_mem_access()
{
	// Different VGA memory access modes.
	// Note that some have higher precedence than others; if Chain4 and Odd/Even write are both set, then Chain4 will be selected
	if (seq[4] & 8) {
		write_access = CHAIN4;
	}
	else if (!(seq[4] & 4)) { // Note: bit has to be 0
		write_access = ODDEVEN;
	}
	else {
		write_access = NORMAL;
	}

	if (gfx[5] & 8) {
		read_access = READMODE_1;
	}
	else if (seq[4] & 8) { // Note: Same bit as write
		read_access = CHAIN4;
	}
	else if (gfx[5] & 0x10) { // Note: Different bit than write
		read_access = ODDEVEN;
	}
	else {
		read_access = NORMAL;
	}

	write_mode = gfx[5] & 3;
	logger_en(debug, "Updating Memory Access Constants: write=%" PRIu8 " [mode=%" PRIu8 "], read=%" PRIu8, write_access, write_mode, read_access);
}

void
vga::complete_redraw()
{
	current_scanline = 0;
	character_scanline = crt[8] & 0x1F;
	current_pixel_panning = pixel_panning;
	//vram_addr = ((crt[0x0C] << 8) | crt[0x0D]) << 2; // Video Address Start is done by planar offset
	framebuffer_offset = 0;

	// On nv2a, the framebuffer address is fetched from PCRTC. The address is already byte-addressed, so it doesn't need the extra multiplication here
	vram_addr = m_machine->get<pcrtc>().read(NV_PCRTC_START);

	// Force a complete redraw of the screen, and to do that, pretend that memory has been written.
	memory_modified = 3;
}

void
vga::change_renderer()
{
	// First things first: check if screen is enabled
	if (((seq[1] & 0x20) == 0) && (attr_index & 0x20)) {
		if (gfx[6] & 1) {
			// graphics mode
			if (gfx[5] & 0x40) {
				// 256 mode (AKA mode 13h)
				renderer = MODE_13H_RENDERER;
				renderer |= attr[0x10] >> 6 & 1;
				complete_redraw();
				return;
			}
			else {
				if (!(gfx[5] & 0x20)) {
					renderer = RENDER_4BPP;
				}
				else {
					nxbx_fatal("Unimplemented gfx mode");
				}
			}
		}
		else {
			// alphanumeric
			renderer = ALPHANUMERIC_RENDERER;
		}
	}
	else {
		renderer = BLANK_RENDERER;
	}
	logger_en(debug, "Change renderer to: %" PRId32, renderer);
	renderer |= (seq[1] >> 3 & 1);
	complete_redraw();
}

void
vga::update_size()
{
	// CR01 and CR02 control width.
	// Technically, CR01 should be less than CR02, but that may not always be the case.
	// Both should be less than CR00
	uint32_t horizontal_display_enable_end = crt[1] + 1;
	uint32_t horizontal_blanking_start = crt[2];
	uint32_t total_horizontal_characters = (horizontal_display_enable_end < horizontal_blanking_start) ? horizontal_display_enable_end : horizontal_blanking_start;
	// Screen width is measured in terms of characters
	uint32_t width = total_horizontal_characters * char_width;

	// CR12 and CR15 control height
	uint32_t vertical_display_enable_end = (crt[0x12] + (((crt[0x07] >> 1 & 1) | (crt[0x07] >> 5 & 2)) << 8)) + 1;
	uint32_t vertical_blanking_start = crt[0x15] + (((crt[0x07] >> 3 & 1) | (crt[0x09] >> 4 & 2)) << 8);
	uint32_t height = vertical_display_enable_end < vertical_blanking_start ? vertical_display_enable_end : vertical_blanking_start;

	display_set_resolution(width, height);

	framebuffer = (uint32_t *)display_get_pixels();

	total_height = height;
	total_width = width;

	vbe_scanlines_modified.resize(total_height);
	memset(vbe_scanlines_modified.data(), 1, total_height);

	scanlines_to_update = height >> 1;
}

void
vga::io_write8(uint32_t addr, const uint8_t data)
{
	if ((addr >= 0x3B0 && addr <= 0x3BF && (misc & 1)) || (addr >= 0x3D0 && addr <= 0x3DF && !(misc & 1))) {
		logger_en(warn, "Ignoring unsupported write to addr=%04" PRIX32 " data=%02" PRIX8 " misc=%02" PRIX8, addr, data, misc);
		return;
	}

	uint8_t diffxor;
	switch (addr)
	{
	case 0x3C0: // Attribute controller register
		if (!(attr_index & 0x80)) {
			// Select attribute index
			diffxor = (attr_index ^ data);
			attr_index = data & 0x7F /* | (attr_index & 0x80) */; // We already know that attr_index is zero
			if (diffxor & 0x20) {
				change_renderer();
			}
			attr_index = data & 0x7F /* | (attr_index & 0x80) */; // We already know that attr_index is zero
		}
		else {
			// Select attribute data
			uint8_t index = attr_index & 0x1F;
			diffxor = attr[index] ^ data;
			if (diffxor) {
				attr[index] = data;
				switch (index)
				{
				case 0x00:
				case 0x01:
				case 0x02:
				case 0x03:
				case 0x04:
				case 0x05:
				case 0x06:
				case 0x07:
				case 0x08:
				case 0x09:
				case 0x0A:
				case 0x0B:
				case 0x0C:
				case 0x0D:
				case 0x0E:
				case 0x0F:
					if (diffxor & 0x3F) {
						change_attr_cache(index);
					}
					break;

				case 16: // Mode Control Register, mostly for text modes
					/*
bit   0  Graphics mode if set, Alphanumeric mode else.
	  1  Monochrome mode if set, color mode else.
	  2  9-bit wide characters if set.
		 The 9th bit of characters C0h-DFh will be the same as
		 the 8th bit. Otherwise it will be the background color.
	  3  If set Attribute bit 7 is blinking, else high intensity.
	  5  (VGA Only) If set the PEL panning register (3C0h index 13h) is
		 temporarily set to 0 from when the line compare causes a wrap around
		 until the next vertical retrace when the register is automatically
		 reloaded with the old value, else the PEL panning register ignores
		 line compares.
	  6  (VGA Only) If set pixels are 8 bits wide. Used in 256 color modes.
	  7  (VGA Only) If set bit 4-5 of the index into the DAC table are taken
		 from port 3C0h index 14h bit 0-1, else the bits in the palette
		 register are used.
				*/
					if (diffxor & ((1 << 0) | // Alphanumeric/Graphical Mode
						//(1 << 5) | // Line Compare Register
						(1 << 6)) // Pixel Width
						) {
						change_renderer(); // Changes between graphics/alphanumeric mode
					}
					if (diffxor & 0x80) {
						for (int i = 0; i < 16; i++) {
							change_attr_cache(i);
						}
					}
					if (diffxor & ((1 << 2) | // Character Width
						(1 << 3) | // Blinking
						(1 << 5)) // Line compare reset PEL Panning
						) {
						complete_redraw();
					}
					logger_en(debug, "Mode Control Register: %02" PRIX8, data);
					break;

				case 17: // Overscan color register break;
					logger_en(debug, "Overscan color (currently unused): %02" PRIX8, data);
					break;

				case 18: // Color Plane Enable
					logger_en(debug, "Color plane enable: %02" PRIX8, data);
					attr[18] &= 0x0F;
					break;

				case 19: // Horizontal PEL Panning Register
					// This register enables you to shift display data "x" pixels to the left.
					// However, in an effort to confuse people, this value is interpreted differently based on graphics mode
					//
					//    pixels to shift left
					// Value 8-dot 9-dot 256 color
					//   0     0     1       0
					//   1     1     2       -
					//   2     2     3       1
					//   3     3     4       -
					//   4     4     5       2
					//   5     5     6       -
					//   6     6     7       3
					//   7     7     8       -
					//   8     -     0       -
					//   9 and above: all undefined
					// Note that due to these restrictions, it's impossible to obscure a full col of characters (and why would you want to do such a thing?)
					if (data > 8) {
						nxbx_fatal("Unknown PEL pixel panning value");
					}
					if (gfx[5] & 0x40) {
						pixel_panning = data >> 1 & 3;
					}
					else {
						pixel_panning = (data & 7) + (char_width & 1);
					}
					logger_en(debug, "Pixel panning: %" PRIX8 " [raw], %" PRIX8 " [effective value]", data, pixel_panning);
					break;

				case 20: // Color Select Register
					logger_en(debug, "Color select register: %02" PRIX8, data);
					if (diffxor & 15) {
						for (int i = 0; i < 16; i++) {
							change_attr_cache(i);
						}
					}
					break;
				}
			}
		}
		attr_index ^= 0x80;
		break;

	case 0x3C2: // Miscellaneous Output Register
		logger_en(debug, "Write VGA miscellaneous register: 0x%02" PRIX8, data);
		/*
bit   0  If set Color Emulation. Base Address=3Dxh else Mono Emulation. Base
		 Address=3Bxh.
	  1  Enable CPU Access to video memory if set
	2-3  Clock Select
		  0: 14MHz(EGA)     25MHz(VGA)
		  1: 16MHz(EGA)     28MHz(VGA)
		  2: External(EGA)  Reserved(VGA)
	  4  (EGA Only) Disable internal video drivers if set
	  5  When in Odd/Even modes Select High 64k bank if set
	  6  Horizontal Sync Polarity. Negative if set
	  7  Vertical Sync Polarity. Negative if set
		 Bit 6-7 indicates the number of lines on the display:
			  0=200(EGA)  Reserved(VGA)
			  1=          400(VGA)
			  2=350(EGA)  350(VGA)
			  3=          480(VGA).
		*/
		misc = data;
		break;

	case 0x3B8:
	case 0x3BF: // ???
	case 0x3C3: // ???
	case 0x3DA:
	case 0x3D8:
	case 0x3CD:
		logger_en(warn, "Unknown write to %x: %02" PRIX8, addr, data);
		break;

	case 0x3C4: // Sequencer Index
		seq_index = data & 7;
		break;

	case 0x3C5: { // Sequencer Data
		const uint8_t mask[8] = {
			// which bits are reserved
			MASK(0b00000000), // 0
			MASK(0b11000010), // 1
			MASK(0b11110000), // 2
			MASK(0b11000000), // 3
			MASK(0b11110001), // 4
			MASK(0b11111111), // 5
			MASK(0b11111111), // 6
			MASK(0b11111111) // 7
		};
		uint8_t data1 = data & mask[seq_index];
		diffxor = seq[seq_index] ^ data1;
		if (diffxor) {
			seq[seq_index] = data1;
			switch (seq_index)
			{
			case 0: // Sequencer Reset
				logger_en(debug, "SEQ: Resetting sequencer");
				break;

			case 1: // Clocking Mode
				logger_en(debug, "SEQ: Setting Clocking Mode to 0x%02" PRIX8, data1);
				if (diffxor & 0x20) { // Screen Off
					change_renderer();
				}
				if (diffxor & 0x08) { // Dot Clock Divide (AKA Fat Screen). Each column will be duplicated
					change_renderer();
					update_size();
				}
				if (diffxor & 0x01) { // 8/9 Dot Clocks
					char_width = 9 ^ (data1 & 1);
					update_size();
					complete_redraw();
				}
				break;

			case 2: // Memory Write Access
				logger_en(debug, "SEQ: Memory plane write access: 0x%02" PRIX8, data1);
				break;

			case 3: // Character Map Select
				// Note these are font addresses in plane 2
				logger_en(debug, "SEQ: Memory plane write access: 0x%02" PRIX8, data1);
				character_map[0] = char_map_address((data1 >> 5 & 1) | (data1 >> 1 & 6));
				character_map[1] = char_map_address((data1 >> 4 & 1) | (data1 << 1 & 6));
				break;

			case 4: // Memory Mode
				logger_en(debug, "SEQ: Memory Mode: 0x%02" PRIX8, data1);
				if (diffxor & 0b1100) {
					update_mem_access();
				}
				break;
			}
		}
	}
	break;

	case 0x3C6: // DAC Palette Mask
		// Used to play around with which colors can be accessed in the 256 DAC cache
		dac_mask = data;
		complete_redraw(); // Doing something as drastic as this deserves a redraw
		break;

	case 0x3C7: // DAC Read Address
		dac_read_address = data;
		dac_color = 0;
		break;

	case 0x3C8: // PEL Address Write Mode
		dac_address = data;
		dac_color = 0;
		break;

	case 0x3C9: // PEL Data Write
		dac_state = 3;
		dac[(dac_address << 2) | dac_color++] = data;
		if (dac_color == 3) { // 0: red, 1: green, 2: blue, 3: ???
			update_one_dac_entry(dac_address);
			dac_address++; // This will wrap around because it is a uint8_t
			dac_color = 0;
		}
		break;

	case 0x3CE: // Graphics Register Index
		gfx_index = data & 15;
		break;

	case 0x3CF: { // Graphics Register Data
		constexpr uint8_t mask[16] = {
			MASK(0b11110000), // 0
			MASK(0b11110000), // 1
			MASK(0b11110000), // 2
			MASK(0b11100000), // 3
			MASK(0b11111100), // 4
			MASK(0b10000100), // 5
			MASK(0b11110000), // 6
			MASK(0b11110000), // 7
			MASK(0b00000000), // 8
			MASK(0b11111111), // 9 - not documented
			MASK(0b00001000), // 10 - ???
			MASK(0b00000000), // 11 - ???
			MASK(0b11111111), // 12 - not documented
			MASK(0b11111111), // 13 - not documented
			MASK(0b11111111), // 14 - not documented
			MASK(0b11111111), // 15 - not documented
			//MASK(0b00000000), // 18 - scratch room vga
		};

		uint8_t data1 = data & mask[gfx_index];
		diffxor = gfx[gfx_index] ^ data1;
		if (diffxor) {
			gfx[gfx_index] = data1;
			switch (gfx_index)
			{
			case 0: // Set/Reset Plane
				logger_en(debug, "Set/Reset Plane: %02" PRIX8, data1);
				break;

			case 1: // Enable Set/Reset Plane
				logger_en(debug, "Enable Set/Reset Plane: %02" PRIX8, data1);
				break;

			case 2: // Color Comare
				logger_en(debug, "Color Compare: %02" PRIX8, data1);
				break;

			case 3: // Data Rotate/ALU Operation Select
				logger_en(debug, "Data Rotate: %02" PRIX8, data1);
				break;

			case 4: // Read Plane Select
				logger_en(debug, "Read Plane Select: %02" PRIX8, data1);
				break;

			case 5: //  Graphics Mode
				logger_en(debug, "Graphics Mode: %02" PRIX8, data1);
				if (diffxor & (3 << 5)) { // Shift Register Control
					change_renderer();
				}
				if (diffxor & ((1 << 3) | (1 << 4) | 3)) {
					update_mem_access();
				}
				break;

			case 6: // Miscellaneous Register
				logger_en(debug, "Miscellaneous Register: %02" PRIX8, data);
				switch (data >> 2 & 3)
				{
				case 0:
					vram_window_base = 0xA0000;
					vram_window_size = 0x20000;
					break;

				case 1:
					vram_window_base = 0xA0000;
					vram_window_size = 0x10000;
					break;

				case 2:
					vram_window_base = 0xB0000;
					vram_window_size = 0x8000;
					break;

				case 3:
					vram_window_base = 0xB8000;
					vram_window_size = 0x8000;
					break;
				}
				if (diffxor & 1) {
					change_renderer();
				}
				break;

			case 7:
				logger_en(debug, "Color Don't Care: %02" PRIX8, data1);
				break;

			case 8:
				logger_en(debug, "Bit Mask Register: %02" PRIX8, data1);
				break;
			}
		}
		break;
	}

	case 0x3D4:
	case 0x3B4: // CRT index
		crt_index = data;
		break;

	case 0x3D5:
	case 0x3B5: { // CRT data
		static uint8_t mask[64] = {
			// 0-7 are changed based on CR11 bit 7
			MASK(0b00000000), // 0
			MASK(0b00000000), // 1
			MASK(0b00000000), // 2
			MASK(0b00000000), // 3
			MASK(0b00000000), // 4
			MASK(0b00000000), // 5
			MASK(0b00000000), // 6
			MASK(0b00000000), // 7
			MASK(0b10000000), // 8
			MASK(0b00000000), // 9
			MASK(0b11000000), // A
			MASK(0b10000000), // B
			MASK(0b00000000), // C
			MASK(0b00000000), // D
			MASK(0b00000000), // E
			MASK(0b00000000), // F
			MASK(0b00000000), // 10
			MASK(0b00110000), // 11
			MASK(0b00000000), // 12
			MASK(0b00000000), // 13
			MASK(0b10000000), // 14
			MASK(0b00000000), // 15
			MASK(0b10000000), // 16
			MASK(0b00010000), // 17
			MASK(0b00000000)  // 18
		};

		// Check extended vga registers separately
		if (crt_index > 0x18) {
			switch (crt_index)
			{
			case 0x1F:
				// Lock register, 0x57 -> unlock, 0x99 -> lock
				if (data == 0x57) {
					crt[0x1F] = 1;
				}
				else if (data == 0x99) {
					crt[0x1F] = 0;
				}
				break;

			default:
				if (crt[0x1F]) {
					crt[crt_index] = data;
				}
			}
			return;
		}
		// The extra difficulty here comes from the fact that the mask is used here to allow masking of CR0-7 in addition to keeping out undefined bits
		uint8_t data1 = data & mask[crt_index];
		// consider the case when we write 0x33 to CR01 (which is currently 0x66) and write protection is on
		// In this case, we would be doing (0x33 & 0) ^ 0x66 which would result in 0x66 being put in diffxor
		// However, if we masked the result, the following would occur: ((0x33 & 0) ^ 0x66) & 0 = 0
		diffxor = (data1 ^ crt[crt_index]) & mask[crt_index];
		if (diffxor) {
			crt[crt_index] = data1 | (crt[crt_index] & ~mask[crt_index]);
			switch (crt_index)
			{
			case 1:
				logger_en(debug, "End Horizontal Display: %02" PRIX8, data1);
				update_size();
				break;

			case 2:
				logger_en(debug, "Start Horizontal Blanking: %02" PRIX8, data1);
				update_size();
				break;

			case 7:
				logger_en(debug, "CRT Overflow: %02" PRIX8, data1);
				update_size();
				break;

			case 9:
				logger_en(debug, "Start Horizontal Blanking: %02" PRIX8, data1);
				if (diffxor & 0x20) {
					update_size();
				}
				break;

			case 0x11:
				if (diffxor & 0x80) {
					uint8_t fill_value = (int8_t)(crt[0x11] ^ 0x80) >> 7;
					for (int i = 0; i < 8; i++) {
						mask[i] = fill_value;
					}
					mask[7] &= ~0x10;
					data1 &= mask[crt_index];
				}
				break;

			case 0x12:
				logger_en(debug, "Vertical Display End: %02" PRIX8, data1);
				update_size();
				break;

			case 0x15:
				logger_en(debug, "Start Vertical Blanking: %02" PRIX8, data1);
				update_size();
				break;
			}
		}
		break;
	}
	default:
		logger_en(warn, "VGA write: 0x%08" PRIX32 " [data: 0x%02" PRIX8 "]", addr, data);
	}
}

void
vga::io_write16(uint32_t addr, const uint16_t data)
{
	io_write8(addr, data & 0xFF);
	io_write8(addr + 1, (data >> 8) & 0xFF);
}

uint8_t
vga::io_read8(uint32_t addr)
{
	if ((addr >= 0x3B0 && addr <= 0x3BF && (misc & 1)) || (addr >= 0x3D0 && addr <= 0x3DF && !(misc & 1))) {
		return 0;
	}

	switch (addr)
	{
	case 0x3C0:
		return attr_index;

	case 0x3C1:
		return attr[attr_index & 0x1F];

	case 0x3C2:
		return misc;

	case 0x3C4:
		return seq_index;

	case 0x3C5:
		return seq[seq_index];

	case 0x3C6:
		return dac_mask;

	case 0x3C7:
		return dac_state;

	case 0x3C8:
		return dac_address;

	case 0x3C9: {
		dac_state = 0;
		uint8_t data = dac[(dac_read_address << 2) | (dac_color++)];
		if (dac_color == 3) {
			dac_read_address++;
			dac_color = 0;
		}
		return data;
	}

	case 0x3CC:
		return misc;

	case 0x3CE:
		return gfx_index;

	case 0x3CF:
		return gfx[gfx_index];

	case 0x3B8:
	case 0x3D8:
	case 0x3CD:
		return -1;

	case 0x3BA:
	case 0x3DA: // Input status Register #1
		// Some programs poll this register to make sure that graphics registers are only being modified during vertical retrace periods
		// Not many programs require this feature to work. For now, we can fake this effect.
		status[1] ^= 9;
		attr_index &= ~0x80; // Also clears attr flip flop
		return status[1];

	case 0x3B5:
	case 0x3D5:
		if ((crt_index > 0x18) && (crt_index != 0x1F) && (crt[0x1F] == 0)) {
			return 0;
		}
		return crt[crt_index];

	default:
		logger_en(warn, "Unknown read: 0x%" PRIX32, addr);
		return -1;
	}
}

void
vga::mem_write8(uint32_t addr, const uint8_t data)
{
	addr -= vram_window_base;
	if (addr > vram_window_size) { // Note: will catch the case where addr < vram_window_base as well
		return;
	}

	int plane = 0, plane_addr = -1;
	switch (write_access)
	{
	case CHAIN4:
		plane = 1 << (addr & 3);
		plane_addr = addr >> 2;
		break;

	case ODDEVEN:
		plane = 5 << (addr & 1);
		plane_addr = addr & ~1;
		break;

	case NORMAL:
		plane = 15; // This will be masked out by SR02 later
		plane_addr = addr;
		break;
	}

	uint32_t data32 = data, and_value = 0xFFFFFFFF; // this will be expanded to 32 bits later, but for now keep it as 8 bits to keep things simple
	int run_alu = 1;
	switch (write_mode)
	{
	case 0:
		data32 = b8to32(alu_rotate(data));
		data32 = do_mask(data32, expand32(gfx[0]), gfx[1]);
		break;

	case 1:
		data32 = latch32; // TODO: endianness
		run_alu = 0;
		break;

	case 2:
		data32 = expand32(data);
		break;

	case 3:
		and_value = b8to32(alu_rotate(data));
		data32 = expand32(gfx[0]);
		break;
	}

	if (run_alu) {
		uint32_t mask = b8to32(gfx[8]) & and_value;
		switch (gfx[3] & 0x18) {
		case 0: // MOV (Unmodified)
			data32 = (data32 & mask) | (latch32 & ~mask);
			break;

			// TODO: Simplify the rest using Boolean Algebra
		case 0x08: // AND
			// ABC + B~C
			// B(AC + ~C)
			// (A + ~C)B
			data32 = ((data32 & latch32) & mask) | (latch32 & ~mask);
			break;

		case 0x10: // OR
			data32 = ((data32 | latch32) & mask) | (latch32 & ~mask);
			break;

		case 0x18: // XOR
			data32 = ((data32 ^ latch32) & mask) | (latch32 & ~mask);
			break;
		}
	}

	if (plane_addr > 65536) {
		nxbx_fatal("Writing outside plane bounds");
	}

	// Actually write to memory
	plane &= seq[2];
	uint32_t *vram_ptr = (uint32_t *)&vram[plane_addr << 2];
	*vram_ptr = do_mask(*vram_ptr, data32, plane);

	// Update scanline
	uint32_t offs = (plane_addr << 2) - m_machine->get<pcrtc>().read(NV_PCRTC_START),
		offset_between_lines = (((crt[0x25] & 0x20) << 6) | ((crt[0x19] & 0xE0) << 3) | crt[0x13]) << 3;

	unsigned int scanline = offs / offset_between_lines;
	if (total_height > scanline) {
		switch (renderer >> 1)
		{
		case MODE_13H_RENDERER >> 1:
			// Determine the scanline that it has been written to.
			vbe_scanlines_modified[scanline] = 1;
			break;

		case RENDER_4BPP >> 1:
			// todo: what about bit13 replacement?
			vbe_scanlines_modified[scanline] = 1;
			break;
		}
	}

	memory_modified = 3;
}

void
vga::mem_write16(uint32_t addr, const uint16_t data)
{
	mem_write8(addr, data & 0xFF);
	mem_write8(addr + 1, (data >> 8) & 0xFF);
}

uint8_t
vga::mem_read8(uint32_t addr)
{
	addr -= vram_window_base;
	if (addr > vram_window_size) { // Note: will catch the case where addr < vram_window_base as well
		return 0;
	}
	// Fill Latches with data from all 4 planes
	// TODO: endianness
	latch32 = ((uint32_t *)(vram))[addr];

	int plane = 0, plane_addr = -1;
	uint8_t color_dont_care[4], color_compare[4];
	switch (read_access)
	{
	case CHAIN4:
		plane = addr & 3;
		plane_addr = addr >> 2;
		break;

	case ODDEVEN:
		plane = (addr & 1) | (gfx[4] & 2);
		plane_addr = addr & ~1;
		break;

	case NORMAL:
		plane = gfx[4] & 3;
		plane_addr = addr;
		break;

	case READMODE_1:
		expand32_alt(color_dont_care, gfx[7]);
		expand32_alt(color_compare, gfx[2]);
		return ~( //
			((latch8[0] & color_dont_care[0]) ^ color_compare[0]) | //
			((latch8[1] & color_dont_care[1]) ^ color_compare[1]) | //
			((latch8[2] & color_dont_care[2]) ^ color_compare[2]) | //
			((latch8[3] & color_dont_care[3]) ^ color_compare[3]));
	}

	if (plane_addr > 65536) {
		nxbx_fatal("Reading outside plane bounds");
	}

	return vram[plane | (plane_addr << 2)];
}

uint16_t
vga::mem_read16(uint32_t addr)
{
	uint16_t result = mem_read8(addr);
	return result | (mem_read8(addr + 1) << 8);
}

void
vga::update()
{
	// TODO: this function is supposed to be called at the refresh rate of the monitor. Currently, it's not being called by anything because
	// there's no gui yet

	// Note: This function should NOT modify any VGA registers or memory!

	framectr = (framectr + 1) & 0x3F;
	uint32_t scanlines_to_update1 = scanlines_to_update; // XXX

	// Text Mode state
	unsigned cursor_scanline_start = 0, cursor_scanline_end = 0, cursor_enabled = 0, cursor_address = 0,
		underline_location = 0, line_graphics = 0;
	// 4BPP renderer
	unsigned enableMask = 0, address_bit_mapping = 0;

	// All non-VBE renderers
	//unsigned offset_between_lines = (((!crt[0x13]) << 8 | crt[0x13]) * 2) << 2;

	// On nv2a, the line offset is derived from the extended vga registers crt[0x19] and crt[0x25]
	unsigned offset_between_lines = (((crt[0x25] & 0x20) << 6) | ((crt[0x19] & 0xE0) << 3) | crt[0x13]) << 3;
	switch (renderer & ~1)
	{
	case BLANK_RENDERER:
		break;

	case ALPHANUMERIC_RENDERER:
		cursor_scanline_start = crt[0x0A] & 0x1F;
		cursor_scanline_end = crt[0x0B] & 0x1F;
		cursor_enabled = (crt[0x0B] & 0x20) || (framectr >= 0x20);
		cursor_address = (crt[0x0E] << 8 | crt[0x0F]) << 2;
		underline_location = crt[0x14] & 0x1F;
		line_graphics = char_width == 9 ? ((attr[0x10] & 4) ? 0xE0 : 0) : 0;
		break;

	case RENDER_4BPP:
		enableMask = attr[0x12] & 15;
		address_bit_mapping = crt[0x17] & 1;
		break;

	case RENDER_16BPP: // VBE 16-bit BPP mode
		offset_between_lines = total_width * 2;
		break;

	case RENDER_24BPP: // VBE 24-bit BPP mode
		offset_between_lines = total_width * 3;
		break;

	case RENDER_32BPP: // VBE 32-bit BPP mode
		offset_between_lines = total_width * 4;
		break;
	}
	if (!memory_modified) {
		return;
	}
	memory_modified &= ~(1 << (int)(current_scanline != 0));

	uint32_t
		//current = current_scanline,
		total_scanlines_drawn = 0;

	while (scanlines_to_update1--) {
		total_scanlines_drawn++;
		// Things to account for here
		//  - Doubling Scanlines
		//  - Character Scanlines
		//  - Line Compare (aka split screen)
		//  - Incrementing & Wrapping Around Scanlines
		//  - Drawing the scanline itself

		// First things first, doubling scanlines
		// On a screen without doubling, the scanlines would look like this:
		//  0: QWERTYUIOPQWERTYUIOPQWERTYUIOP
		//  1: ASDFGHJKLASDFGHJKLASDFGHJKLASD
		//  2: ZXCVBNMZXCVBNMZXCVBNMZXCVBNMZX
		//  3: ...
		// with scanline doubling, however, it looks like this:
		//  0: QWERTYUIOPQWERTYUIOPQWERTYUIOP
		//  1: QWERTYUIOPQWERTYUIOPQWERTYUIOP <-- dupe
		//  2: ASDFGHJKLASDFGHJKLASDFGHJKLASD
		//  3: ASDFGHJKLASDFGHJKLASDFGHJKLASD <-- dupe
		//  4: ZXCVBNMZXCVBNMZXCVBNMZXCVBNMZX
		//  5: ZXCVBNMZXCVBNMZXCVBNMZXCVBNMZX <-- dupe
		//  6: ...
		//  7: (same as #6)
		// Therefore, we can come to the conclusion that if scanline doubling is enabled, then all odd scanlines are simply copies of the one preceding them
		if ((current_scanline & 1) && (crt[9] & 0x80)) {
			// See above for
			memcpy(&framebuffer[framebuffer_offset], &framebuffer[framebuffer_offset - total_width], total_width);
		}
		else {
			if (current_scanline < total_height) {
				uint32_t fboffset = framebuffer_offset;
				uint32_t vram_addr1 = vram_addr;
				switch (renderer) {
				case BLANK_RENDERER:
				case BLANK_RENDERER | 1:
					for (unsigned int i = 0; i < total_width; i++) {
						framebuffer[fboffset + i] = 255 << 24;
					}
					break;

				case ALPHANUMERIC_RENDERER: {
					// Text Mode Memory Layout (physical)
					// Plane 0: CC XX CC XX
					// Plane 1: AA XX AA XX
					// Plane 2: FF XX FF XX
					// Plane 3: XX XX XX XX
					// In a row: CC AA FF XX XX XX XX XX CC AA FF XX XX XX XX XX
					for (unsigned i = 0; i < total_width; i += char_width, vram_addr1 += 4) {
						uint8_t character = vram[vram_addr1 << 1];
						uint8_t attribute = vram[(vram_addr1 << 1) + 1];
						uint8_t font = vram[( //
							( //
								character_scanline // Current character scanline
								+ character * 32 // Each character holds 32 bytes of font data in plane 2
								+ character_map[~attribute >> 3 & 1]) // Offset in plane to, decided by attribute byte
							<< 2)
							+ 2 // Select Plane 2
						];
						// Determine Color
						uint32_t fg = attribute & 15, bg = attribute >> 4 & 15;

						// Now we can begin to apply special character effects like:
						//  - Cursor
						//  - Blinking
						//  - Underline
						if (cursor_enabled && vram_addr1 == cursor_address) {
							if ((character_scanline >= cursor_scanline_start) && (character_scanline <= cursor_scanline_end)) {
								// cursor is enabled
								bg = fg;
							}
						}

						// TODO: I've noticed that blinking is twice as slow as cursor blinks
						if ((attr[0x10] & 8) && (framectr >= 32)) {
							bg &= 7; // last bit is not interpreted
							if (attribute & 0x80) {
								fg = bg;
							}
						}
						// Underline is simple
						if ((attribute & 0b01110111) == 1) {
							if (character_scanline == underline_location) {
								bg = fg;
							}
						}

						// To draw the character quickly, use a method similar to do_mask
						fg = dac_palette[dac_mask & attr_palette[fg]];
						bg = dac_palette[dac_mask & attr_palette[bg]];
						uint32_t xorvec = fg ^ bg;
						// The following is equivalent to the following:
						//  if(font & bit) framebuffer[fboffset] = fg; else framebuffer[fboffset] = bg;
						framebuffer[fboffset + 0] = ((xorvec & -(font >> 7))) ^ bg;
						framebuffer[fboffset + 1] = ((xorvec & -(font >> 6 & 1))) ^ bg;
						framebuffer[fboffset + 2] = ((xorvec & -(font >> 5 & 1))) ^ bg;
						framebuffer[fboffset + 3] = ((xorvec & -(font >> 4 & 1))) ^ bg;
						framebuffer[fboffset + 4] = ((xorvec & -(font >> 3 & 1))) ^ bg;
						framebuffer[fboffset + 5] = ((xorvec & -(font >> 2 & 1))) ^ bg;
						framebuffer[fboffset + 6] = ((xorvec & -(font >> 1 & 1))) ^ bg;
						framebuffer[fboffset + 7] = ((xorvec & -(font >> 0 & 1))) ^ bg;

						if ((character & line_graphics) == 0xC0) {
							framebuffer[fboffset + 8] = ((xorvec & -(font >> 0 & 1))) ^ bg;
						}
						else if (char_width == 9) {
							framebuffer[fboffset + 8] = bg;
						}
						fboffset += char_width;
					}	
				}
				break;

				case MODE_13H_RENDERER: {
					//if(!vbe_scanlines_modified[current_scanline]) break;
					// CHAIN4 Memory Layout:
					//  Plane 0: AA 00 00 00 AA 00 00 00
					//  Plane 1: BB 00 00 00 BB 00 00 00
					//  Plane 2: CC 00 00 00 CC 00 00 00
					//  Plane 3: DD 00 00 00 DD 00 00 00
					// Draw four clumps of pixels together
					// XXX: What if screen isn't a multiple of four pixels wide?
					for (unsigned i = 0; i < total_width; i += 4, vram_addr1 += 16) {
						for (int j = 0; j < 4; j++) { // hopefully, compiler unrolls loop
							framebuffer[fboffset + j] = dac_palette[vram[vram_addr1 | j] & dac_mask];
						}
						fboffset += 4;
					}
					//vbe_scanlines_modified[current_scanline] = 0;	
				}
				break;

				case MODE_13H_RENDERER | 1:
					//if(!vbe_scanlines_modified[current_scanline]) break;
					for (unsigned int i = 0; i < total_width; i += 8, vram_addr1 += 4) {
						for (int j = 0, k = 0; j < 4; j++, k += 2) {
							framebuffer[fboffset + k] = framebuffer[fboffset + k + 1] = dac_palette[vram[vram_addr1 | j] & dac_mask];
						}
						fboffset += 8;
					}
					//vbe_scanlines_modified[current_scanline] = 0;
					break;

				case RENDER_4BPP: {
					//if(!vbe_scanlines_modified[current_scanline]) break;
					uint32_t addr = vram_addr1;
					if (character_scanline & address_bit_mapping) {
						addr |= 0x8000;
					}
					uint8_t p0 = vram[addr | 0];
					uint8_t p1 = vram[addr | 1];
					uint8_t p2 = vram[addr | 2];
					uint8_t p3 = vram[addr | 3];

					for (unsigned x = 0, px = current_pixel_panning; x < total_width; x++, fboffset++, px++) {
						if (px > 7) {
							px = 0;
							addr += 4;
							p0 = vram[addr | 0];
							p1 = vram[addr | 1];
							p2 = vram[addr | 2];
							p3 = vram[addr | 3];
						}
						int pixel = bpp4_to_offset(p0, px, 0) | bpp4_to_offset(p1, px, 1) | bpp4_to_offset(p2, px, 2) | bpp4_to_offset(p3, px, 3);
						pixel &= enableMask;
						framebuffer[fboffset] = dac_palette[dac_mask & attr_palette[pixel]];
					}
					//vbe_scanlines_modified[current_scanline] = 0;
					break;
				}
				case RENDER_4BPP | 1: {
					// 4BPP rendering mode, but lower resolution
					//if(!vbe_scanlines_modified[current_scanline]) break;
					uint32_t addr = vram_addr1;
					uint8_t p0 = vram[addr | 0];
					uint8_t p1 = vram[addr | 1];
					uint8_t p2 = vram[addr | 2];
					uint8_t p3 = vram[addr | 3];
					for (unsigned int x = 0, px = current_pixel_panning; x < total_width; x += 2, fboffset += 2, px++) {
						if (px > 7) {
							px = 0;
							addr += 4;
							p0 = vram[addr | 0];
							p1 = vram[addr | 1];
							p2 = vram[addr | 2];
							p3 = vram[addr | 3];
						}
						int pixel = bpp4_to_offset(p0, px, 0) | bpp4_to_offset(p1, px, 1) | bpp4_to_offset(p2, px, 2) | bpp4_to_offset(p3, px, 3);
						pixel &= enableMask;
						uint32_t result = dac_palette[dac_mask & attr_palette[pixel]];
						framebuffer[fboffset] = result;
						framebuffer[fboffset + 1] = result;
					}
					//vbe_scanlines_modified[current_scanline] = 0;
				}
				break;

				case RENDER_32BPP:
					if (!vbe_scanlines_modified[current_scanline]) {
						break;
					}
					for (unsigned int i = 0; i < total_width; i++, vram_addr1 += 4) {
						framebuffer[fboffset++] = *((uint32_t *)&vram[vram_addr1]) | 0xFF000000;
					}
					vbe_scanlines_modified[current_scanline] = 0;
					break;

				case RENDER_8BPP:
					if (!vbe_scanlines_modified[current_scanline]) {
						break;
					}
					for (unsigned int i = 0; i < total_width; i++, vram_addr1++) {
						framebuffer[fboffset++] = dac_palette[vram[vram_addr1]];
					}
					vbe_scanlines_modified[current_scanline] = 0;
					break;

				case RENDER_16BPP:
					if (!vbe_scanlines_modified[current_scanline]) {
						break;
					}
					for (unsigned int i = 0; i < total_width; i++, vram_addr1 += 2) {
						uint16_t word = *((uint16_t *)&vram[vram_addr1]);
						int red = word >> 11 << 3, green = (word >> 5 & 63) << 2, /* Note: 6 bits for green */ blue = (word & 31) << 3;
						framebuffer[fboffset++] = red << 16 | green << 8 | blue << 0 | 0xFF000000;
					}

					vbe_scanlines_modified[current_scanline] = 0;
					break;

				case RENDER_24BPP:
					if (!vbe_scanlines_modified[current_scanline]) {
						break;
					}
					for (uint32_t i = 0; i < total_width; i++, vram_addr1 += 3) {
						uint8_t blue = vram[vram_addr1], green = vram[vram_addr1 + 1], red = vram[vram_addr1 + 2];
						framebuffer[fboffset++] = (blue) | (green << 8) | (red << 16) | 0xFF000000;
					}
					vbe_scanlines_modified[current_scanline] = 0;
					break;
				}
				if ((crt[9] & 0x1F) == character_scanline) {
					character_scanline = 0;
					vram_addr += offset_between_lines; // TODO: Dword Mode
				}
				else {
					character_scanline++;
				}
			}
		}
		current_scanline = (current_scanline + 1) & 0x0FFF; // Increment current scan line
		framebuffer_offset += total_width;
		if (current_scanline >= total_height) {
			// Technically, we should draw output to the value specified by the CRT Vertical Total Register, but why bother?

			// Update the display when all the scanlines have been drawn
			display_update();

			complete_redraw(); // contrary to its name, it only resets drawing state
			//current = 0;

			total_scanlines_drawn = 0;

			// also, one frame has been completely drawn
			//framectr = (framectr + 1) & 0x3F;
		}
	}
}

void
vga::reset()
{
	std::fill(crt, &crt[256], 0);
	crt_index = 0;
	std::fill(attr, &attr[32], 0);
	attr_index = 0;
	std::fill(attr_palette, &attr_palette[16], 0);
	std::fill(seq, &seq[8], 0);
	seq_index = 0;
	std::fill(gfx, &gfx[256], 0);
	gfx_index = 0;
	std::fill(dac, &dac[1024], 0);
	std::fill(dac_palette, &dac_palette[256], 0 );
	dac_mask = dac_state = dac_address = dac_color = dac_read_address = 0;
	std::fill(status, &status[2], 0);
	misc = 1; // set to 1 because Direct3D_CreateDevice attempts to access port 0x3D4 without setting this register first
	char_width = 9; // default size of SR01 bit 0 is 0
	std::fill(character_map, &character_map[2], 0);
	pixel_panning = current_pixel_panning = 0;
	total_height = total_width = 0;
	renderer = 0;
	current_scanline = character_scanline = 0;
	framebuffer = nullptr;
	framebuffer_offset = 0;
	vram_addr = 0;
	scanlines_to_update = 0;
	write_access = read_access = write_mode = 0;
	vram_window_base = vram_window_size = 0;
	latch32 = 0;
	framectr = 0;
	memory_modified = 0;
	vbe_scanlines_modified.resize(0);
	complete_redraw();
}

bool
vga::init()
{
	vram_size = m_machine->get<cpu>().get_ramsize();
	vram = get_ram_ptr(m_machine->get<cpu_t *>());
	
	reset();
	return true;
}
