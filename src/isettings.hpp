// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2026 ergo720

#pragma once

#include <cstdint>
#include <vector>
#include <any>
#include <string_view>


class isettings {
public:
	virtual bool init(std::string_view ini_path) = 0;
	virtual void save() = 0;
	virtual long get_long_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault = 0) = 0;
	virtual uint32_t get_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault = 0) = 0;
	virtual int64_t get_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_nDefault = 0) = 0;
	virtual float get_float_value(const char *a_pSection, const char *a_pKey, float a_nDefault = 0) = 0;
	virtual std::vector<std::any> get_vector_values(const char *a_pSection, const char *a_pKey) = 0;
	virtual void set_long_value(const char *a_pSection, const char *a_pKey, long a_pValue, bool a_bUseHex = false) = 0;
	virtual void set_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_pValue, bool a_bUseHex = false) = 0;
	virtual void set_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_pValue, bool a_bUseHex = false) = 0;
	virtual void set_float_value(const char *a_pSection, const char *a_pKey, float a_pValue) = 0;
	virtual void set_vector_values(const char *a_pSection, const char *a_pKey, std::vector<std::any> a_pValue) = 0;
};
