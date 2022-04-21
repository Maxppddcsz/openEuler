// SPDX-License-Identifier: GPL-2.0
/*
 * This file implements the error recovery as a core part of PCIe error
 * reporting. When a PCIe error is delivered, an error message will be
 * collected and printed to console, then, an error recovery procedure
 * will be executed by following the PCI error recovery rules.
 *
 * Copyright (C) 2006 Intel Corp.
 *	Tom Long Nguyen (tom.l.nguyen@intel.com)
 *	Zhang Yanmin (yanmin.zhang@intel.com)
 */

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/aer.h>
#include "portdrv.h"
#include "../pci.h"

static pci_ers_result_t merge_result(enum pci_ers_result orig,
				  enum pci_ers_result new)
{
	if (new == PCI_ERS_RESULT_NO_AER_DRIVER)
		return PCI_ERS_RESULT_NO_AER_DRIVER;

	if (new == PCI_ERS_RESULT_NONE)
		return orig;

	switch (orig) {
	case PCI_ERS_RESULT_CAN_RECOVER:
	case PCI_ERS_RESULT_RECOVERED:
		orig = new;
		break;
	case PCI_ERS_RESULT_DISCONNECT:
		if (new == PCI_ERS_RESULT_NEED_RESET)
			orig = PCI_ERS_RESULT_NEED_RESET;
		break;
	default:
		break;
	}

	return orig;
}

static int report_error_detected(struct pci_dev *dev,
				 enum pci_channel_state state,
				 enum pci_ers_result *result)
{
	pci_ers_result_t vote;
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	if (!pci_dev_set_io_state(dev, state) ||
		!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->error_detected) {
		/*
		 * If any device in the subtree does not have an error_detected
		 * callback, PCI_ERS_RESULT_NO_AER_DRIVER prevents subsequent
		 * error callbacks of "any" device in the subtree, and will
		 * exit in the disconnected error state.
		 */
		if (dev->hdr_type != PCI_HEADER_TYPE_BRIDGE) {
			vote = PCI_ERS_RESULT_NO_AER_DRIVER;
			pci_info(dev, "AER: Can't recover (no error_detected callback)\n");
		} else {
			vote = PCI_ERS_RESULT_NONE;
		}
	} else {
		err_handler = dev->driver->err_handler;
		vote = err_handler->error_detected(dev, state);
	}
	pci_uevent_ers(dev, vote);
	*result = merge_result(*result, vote);
	device_unlock(&dev->dev);
	return 0;
}

static int report_frozen_detected(struct pci_dev *dev, void *data)
{
	return report_error_detected(dev, pci_channel_io_frozen, data);
}

static int report_normal_detected(struct pci_dev *dev, void *data)
{
	return report_error_detected(dev, pci_channel_io_normal, data);
}

static int report_mmio_enabled(struct pci_dev *dev, void *data)
{
	pci_ers_result_t vote, *result = data;
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	if (!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->mmio_enabled)
		goto out;

	err_handler = dev->driver->err_handler;
	vote = err_handler->mmio_enabled(dev);
	*result = merge_result(*result, vote);
out:
	device_unlock(&dev->dev);
	return 0;
}

static int report_slot_reset(struct pci_dev *dev, void *data)
{
	pci_ers_result_t vote, *result = data;
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	if (!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->slot_reset)
		goto out;

	err_handler = dev->driver->err_handler;
	vote = err_handler->slot_reset(dev);
	*result = merge_result(*result, vote);
out:
	device_unlock(&dev->dev);
	return 0;
}

static int report_resume(struct pci_dev *dev, void *data)
{
	const struct pci_error_handlers *err_handler;

	device_lock(&dev->dev);
	if (!pci_dev_set_io_state(dev, pci_channel_io_normal) ||
		!dev->driver ||
		!dev->driver->err_handler ||
		!dev->driver->err_handler->resume)
		goto out;

	err_handler = dev->driver->err_handler;
	err_handler->resume(dev);
out:
	pci_uevent_ers(dev, PCI_ERS_RESULT_RECOVERED);
	device_unlock(&dev->dev);
	return 0;
}

/**
 * pci_walk_bridge - walk bridges potentially AER affected
 * @bridge:	bridge which may be a Port, an RCEC, or an RCiEP
 * @cb:		callback to be called for each device found
 * @userdata:	arbitrary pointer to be passed to callback
 *
 * If the device provided is a bridge, walk the subordinate bus, including
 * any bridged devices on buses under this bus.  Call the provided callback
 * on each device found.
 *
 * If the device provided has no subordinate bus, e.g., an RCEC or RCiEP,
 * call the callback on the device itself.
 */
static void pci_walk_bridge(struct pci_dev *bridge,
			    int (*cb)(struct pci_dev *, void *),
			    void *userdata)
{
	if (bridge->subordinate)
		pci_walk_bus(bridge->subordinate, cb, userdata);
	else
		cb(bridge, userdata);
}

/**
 * default_reset_link - default reset function
 * @dev: pointer to pci_dev data structure
 *
 * Invoked when performing link reset on a Downstream Port or a
 * Root Port with no aer driver.
 */
static pci_ers_result_t default_reset_link(struct pci_dev *dev)
{
	int rc;

	rc = pci_bus_error_reset(dev);
	pci_printk(KERN_DEBUG, dev, "downstream link has been reset\n");
	return rc ? PCI_ERS_RESULT_DISCONNECT : PCI_ERS_RESULT_RECOVERED;
}

static pci_ers_result_t reset_link(struct pci_dev *dev, u32 service)
{
	pci_ers_result_t status;
	struct pcie_port_service_driver *driver = NULL;

	driver = pcie_port_find_service(dev, service);
	if (driver && driver->reset_link) {
		status = driver->reset_link(dev);
	} else if (pcie_downstream_port(dev)) {
		status = default_reset_link(dev);
	} else {
		pci_printk(KERN_DEBUG, dev, "no link-reset support at upstream device %s\n",
			pci_name(dev));
		return PCI_ERS_RESULT_DISCONNECT;
	}

	if (status != PCI_ERS_RESULT_RECOVERED) {
		pci_printk(KERN_DEBUG, dev, "link reset at upstream device %s failed\n",
			pci_name(dev));
		return PCI_ERS_RESULT_DISCONNECT;
	}

	return status;
}

pci_ers_result_t pcie_do_recovery(struct pci_dev *dev, enum pci_channel_state state,
		      u32 service)
{
	int type = pci_pcie_type(dev);
	struct pci_dev *bridge;
	pci_ers_result_t status = PCI_ERS_RESULT_CAN_RECOVER;
	struct pci_host_bridge *host = pci_find_host_bridge(dev->bus);

	/*
	 * If the error was detected by a Root Port, Downstream Port, RCEC,
	 * or RCiEP, recovery runs on the device itself.  For Ports, that
	 * also includes any subordinate devices.
	 *
	 * If it was detected by another device (Endpoint, etc), recovery
	 * runs on the device and anything else under the same Port, i.e.,
	 * everything under "bridge".
	 */
	if (type == PCI_EXP_TYPE_ROOT_PORT ||
	    type == PCI_EXP_TYPE_DOWNSTREAM ||
	    type == PCI_EXP_TYPE_RC_EC ||
	    type == PCI_EXP_TYPE_RC_END)
		bridge = dev;
	else
		bridge = pci_upstream_bridge(dev);

	pci_dbg(bridge, "broadcast error_detected message\n");
	if (state == pci_channel_io_frozen) {
		pci_walk_bridge(bridge, report_frozen_detected, &status);
		status = reset_link(bridge, service);
		if (status != PCI_ERS_RESULT_RECOVERED)
			goto failed;
	} else {
		pci_walk_bridge(bridge, report_normal_detected, &status);
	}

	if (status == PCI_ERS_RESULT_CAN_RECOVER) {
		status = PCI_ERS_RESULT_RECOVERED;
		pci_dbg(bridge, "broadcast mmio_enabled message\n");
		pci_walk_bridge(bridge, report_mmio_enabled, &status);
	}

	if (status == PCI_ERS_RESULT_NEED_RESET) {
		/*
		 * TODO: Should call platform-specific
		 * functions to reset slot before calling
		 * drivers' slot_reset callbacks?
		 */
		status = PCI_ERS_RESULT_RECOVERED;
		pci_dbg(bridge, "broadcast slot_reset message\n");
		pci_walk_bridge(bridge, report_slot_reset, &status);
	}

	if (status != PCI_ERS_RESULT_RECOVERED)
		goto failed;

	pci_dbg(bridge, "broadcast resume message\n");
	pci_walk_bridge(bridge, report_resume, &status);

	/*
	 * If we have native control of AER, clear error status in the Root
	 * Port or Downstream Port that signaled the error.  If the
	 * platform retained control of AER, it is responsible for clearing
	 * this status.  In that case, the signaling device may not even be
	 * visible to the OS.
	 */
	if (host->native_aer || pcie_ports_native) {
		pcie_clear_device_status(bridge);
		pci_cleanup_aer_uncorrect_error_status(bridge);
	}
	pci_info(bridge, "AER: Device recovery successful\n");
	return status;

failed:
	pci_uevent_ers(bridge, PCI_ERS_RESULT_DISCONNECT);

	/* TODO: Should kernel panic here? */
	pci_info(bridge, "AER: Device recovery failed\n");

	return status;
}
