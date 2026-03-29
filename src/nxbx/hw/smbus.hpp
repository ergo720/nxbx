// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 ergo720

#pragma once

#include <cstdint>
#include <memory>

class machine;

class smbus
{
public:
	smbus();
	~smbus();
	bool init(machine *machine);
	void deinit();
	void reset();
	void updateIoLogging();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
