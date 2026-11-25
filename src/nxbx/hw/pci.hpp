/*
* SPDX-License-Identifier: GPL-3.0-only
*
* ergo720                Copyright (c) 2024
*
*  This program is free software: you can redistribute it and/or modify it under the terms of the
*  GNU General Public License as published by the Free Software Foundation, version 3.
*
*  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*  See the GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along with this program.
*  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstdint>
#include <memory>


class machine;

class pci
{
public:
	pci();
	~pci();
	void init(machine *machine);
	void reset();
	void updateIoLogging();
	uint8_t *getConfigurationPtr(uint32_t bus, uint32_t device, uint32_t function);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
