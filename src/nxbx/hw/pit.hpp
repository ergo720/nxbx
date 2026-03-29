// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <memory>


class machine;
class cpu;

class pit
{
public:
	pit();
	~pit();
	bool init(machine *machine);
	void reset();
	void updateIoLogging();
	uint64_t getNextIrqTime(uint64_t now);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
