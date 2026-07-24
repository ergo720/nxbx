// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#pragma once

#include <memory>


class machine;

class lpcbridge
{
public:
	lpcbridge();
	~lpcbridge();
	void init(machine *machine);
	void reset();
	void updateIoLogging();
	void triggerInterrupt();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
