// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#include "util.hpp"
#if defined(_MSC_VER)
#include <intrin.h>
#endif


namespace util {
	uint64_t
	muldiv128(uint64_t a, uint64_t b, uint64_t c)
	{
		// Calculates (a * b) / c with a 128 bit multiplication and division to avoid overflow in the intermediate product

#if defined(_WIN64) && defined(_MSC_VER) && !defined(_M_ARM64) && !defined(_M_ARM64EC)
		uint64_t hp, lp, r;
		lp = _umul128(a, b, &hp);
		return _udiv128(hp, lp, c, &r);
#elif defined(__SIZEOF_INT128__)
		return (__uint128_t)a * b / c;
#else
#error "Don't know how to do 128 bit multiplication and division on this platform"
#endif
	}

	char
	xbox_toupper(char c)
	{
		uint8_t char_code = c;

		if (char_code >= 'a' && char_code <= 'z') {
			char_code ^= 0x20;
		}
		else if ((char_code >= 0xE0) && (char_code <= 0xFE) && (char_code != 0xF7)) {
			char_code ^= 0x20;
		}
		else if (char_code == 0xFF) {
			char_code = '?';
		}

		return char_code;
	}
}
