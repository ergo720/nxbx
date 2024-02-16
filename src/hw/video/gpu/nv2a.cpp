// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "pbus_pci.hpp"
#include "pfb.hpp"
#include "pmc.hpp"


void
nv2a_init()
{
	pmc_init();
	pbus_pci_init();
	pfb_init();
}
