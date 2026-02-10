// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include "SimpleIni.h"
#include "isettings.hpp"
#include "nxbx.hpp"
#include "lib86cpu.h"
#include <string>


class settings : public isettings {
public:
	static settings &get()
	{
		static settings m_settings;
		return m_settings;
	}
	settings(settings const &) = delete;
	void operator=(settings const &) = delete;
	bool init(std::string_view ini_path) override;
	void save() override;
	long get_long_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault = 0) override;
	uint32_t get_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_nDefault = 0) override;
	int64_t get_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_nDefault = 0) override;
	float get_float_value(const char *a_pSection, const char *a_pKey, float a_nDefault = 0) override;
	std::vector<std::any> get_vector_values(const char *a_pSection, const char *a_pKey) override;
	void set_long_value(const char *a_pSection, const char *a_pKey, long a_pValue = 0, bool a_bUseHex = false) override;
	void set_uint32_value(const char *a_pSection, const char *a_pKey, uint32_t a_pValue, bool a_bUseHex = false) override;
	void set_int64_value(const char *a_pSection, const char *a_pKey, int64_t a_pValue, bool a_bUseHex = false) override;
	void set_float_value(const char *a_pSection, const char *a_pKey, float a_pValue) override;
	void set_vector_values(const char *a_pSection, const char *a_pKey, std::vector<std::any> a_pValue) override;


private:
	settings() {}
	void reset();

	CSimpleIniA m_ini;
	std::string m_path;
	static constexpr uint32_t m_ini_version = 2;
	static constexpr uint32_t m_log_version = 2; // add one to this every time the log modules change
};
