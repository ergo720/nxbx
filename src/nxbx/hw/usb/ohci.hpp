// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2025 ergo720

#pragma once

#include <cstdint>
#include <memory>


class machine;

class usb0
{
public:
	usb0();
	~usb0();
	void init(machine *machine);
	void reset();
	void updateIoLogging();
	uint64_t getNextUpdateTime(uint64_t now);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
