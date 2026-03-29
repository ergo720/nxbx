// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include <cstdint>
#include <memory>


class machine;

class pic
{
public:
	pic();
	~pic();
	bool init(machine *machine, unsigned idx);
	void reset();
	void updateIoLogging();
	void raiseIrq(uint8_t a);
	void lowerIrq(uint8_t a);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
