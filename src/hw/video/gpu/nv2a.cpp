// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "nv2a.hpp"


void
nv2a_init()
{
	pmc_init();
	pbus_pci_init();
	pfb_init();
	pcrtc_init();
}
