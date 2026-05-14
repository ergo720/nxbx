// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#pragma once

#include "graphics_api.hpp"
#include <memory>
#include <string>


class Vulkan : public GraphicsAPI
{
public:
	Vulkan();
	~Vulkan();
	void init(const std::string &nxbx_dir, uint8_t *ram_ptr, uint64_t ram_size) override;
	void deinit() override;
	void setValidationLayers(uint32_t enable);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
