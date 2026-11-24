/*
* SPDX-License-Identifier: GPL-3.0-only
*
* ergo720                Copyright (c) 2026
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

#include <memory>


class machine;

class hostbridge
{
public:
	hostbridge();
	~hostbridge();
	void init(machine *machine);
	void reset();

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};
