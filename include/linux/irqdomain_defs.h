/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/kabi.h>

#ifndef _LINUX_IRQDOMAIN_DEFS_H
#define _LINUX_IRQDOMAIN_DEFS_H

/*
 * Should several domains have the same device node, but serve
 * different purposes (for example one domain is for PCI/MSI, and the
 * other for wired IRQs), they can be distinguished using a
 * bus-specific token. Most domains are expected to only carry
 * DOMAIN_BUS_ANY.
 */
enum irq_domain_bus_token {
	DOMAIN_BUS_ANY		= 0,
	DOMAIN_BUS_WIRED,
	DOMAIN_BUS_GENERIC_MSI,
	DOMAIN_BUS_PCI_MSI,
	DOMAIN_BUS_PLATFORM_MSI,
	DOMAIN_BUS_NEXUS,
	DOMAIN_BUS_IPI,
	DOMAIN_BUS_FSL_MC_MSI,
	DOMAIN_BUS_TI_SCI_INTA_MSI,
	DOMAIN_BUS_WAKEUP,
	DOMAIN_BUS_VMD_MSI,
	DOMAIN_BUS_PCI_DEVICE_MSI,
	DOMAIN_BUS_PCI_DEVICE_MSIX,
	DOMAIN_BUS_DMAR,
	DOMAIN_BUS_AMDVI,
	DOMAIN_BUS_PCI_DEVICE_IMS,
#ifdef CONFIG_KABI_RESERVE
	DOMAIN_BUS_TYPE_RESERVE_1,
	DOMAIN_BUS_TYPE_RESERVE_2,
	DOMAIN_BUS_TYPE_RESERVE_3,
	DOMAIN_BUS_TYPE_RESERVE_4,
	DOMAIN_BUS_TYPE_RESERVE_5,
	DOMAIN_BUS_TYPE_RESERVE_6,
	DOMAIN_BUS_TYPE_RESERVE_7,
	DOMAIN_BUS_TYPE_RESERVE_8,
#endif
};

#endif /* _LINUX_IRQDOMAIN_DEFS_H */
