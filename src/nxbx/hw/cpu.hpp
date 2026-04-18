// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2023 ergo720

#pragma once

#include "host.hpp"
#include <memory>

#define RAM_SIZE64 0x4000000 // = 64 MiB
#define RAM_SIZE128 0x8000000 // = 128 MiB


class machine;
class cpu_t;

class cpu
{
public:
	cpu();
	~cpu();
	void init(const boot_params &params, machine *machine);
	void deinit();
	void reset();
	void start();
	void exit();
	void updateIoLogging();
	uint64_t checkPeriodicEvents(uint64_t now);
	cpu_t *get86cpu();
	uint32_t getRamsize();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

template<typename D, typename T, auto f, uint32_t base = 0>
T cpu_read(uint32_t addr, void *opaque)
{
	D *device = static_cast<D *>(opaque);
	return (device->*f)(addr - base);
}

template<typename D, typename T, auto f, uint32_t base = 0>
void cpu_write(uint32_t addr, const T value, void *opaque)
{
	D *device = static_cast<D *>(opaque);
	(device->*f)(addr - base, value);
}
