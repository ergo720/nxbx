// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <string>


uint64_t muldiv128_(uint64_t a, uint64_t b, uint64_t c);
char xbox_toupper(char c);

// Case-insensitive variant of std::char_traits<char>, used to compare xbox strings
struct xbox_char_traits : public std::char_traits<char> {
	static bool eq(char c1, char c2)
	{
		return xbox_toupper(c1) == xbox_toupper(c2);
	}

	static bool ne(char c1, char c2)
	{
		return xbox_toupper(c1) != xbox_toupper(c2);
	}

	static bool lt(char c1, char c2)
	{
		return xbox_toupper(c1) < xbox_toupper(c2);
	}

	static int compare(const char *s1, const char *s2, size_t n)
	{
		while (n-- != 0) {
			if (xbox_toupper(*s1) < xbox_toupper(*s2)) {
				return -1;
			}

			if (xbox_toupper(*s1) > xbox_toupper(*s2)) {
				return 1;
			}

			++s1;
			++s2;
		}

		return 0;
	}

	static const char *find(const char *s, size_t n, char a) {
		while ((n-- > 0) && (xbox_toupper(*s) != xbox_toupper(a))) {
			++s;
		}

		return s;
	}
};

template<class DstTraits, class CharT, class SrcTraits>
constexpr std::basic_string_view<CharT, DstTraits>
traits_cast(const std::basic_string_view<CharT, SrcTraits> src) noexcept
{
	return { src.data(), src.size() };
}

using xbox_string_view = std::basic_string_view<char, xbox_char_traits>;
using xbox_string = std::basic_string<char, xbox_char_traits>;
