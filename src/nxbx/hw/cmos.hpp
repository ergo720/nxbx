// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <memory>


class machine;
class cpu;

class cmos
{
public:
	cmos();
	~cmos();
	void init(machine *machine);
	void deinit();
	void reset();
	void updateIoLogging();
	uint64_t getNextUpdateTime(uint64_t now);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
