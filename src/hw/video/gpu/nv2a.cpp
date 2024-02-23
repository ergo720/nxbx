// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 ergo720

#include "nv2a.hpp"


void
nv2a_init()
{
	pmc_init();
	pramdac_init();
	pbus_pci_init();
	pfb_init();
	pcrtc_init();
	ptimer_init();
}

uint64_t
nv2a_get_next_update_time(uint64_t now)
{
	return ptimer_get_next_alarm_time(now);
}
