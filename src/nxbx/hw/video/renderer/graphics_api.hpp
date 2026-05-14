// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#pragma once

#include <cstdint>
#include <string>


class GraphicsAPI
{
public:
	virtual void init(const std::string &nxbx_dir, uint8_t *ram_ptr, uint64_t ram_size) = 0;
	virtual void deinit() = 0;
};
