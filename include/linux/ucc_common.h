/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _UCC_COMMON_H
#define _UCC_COMMON_H

/*
 * UCC Print Function
 */
#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define ucc_err(fmt, ...)	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)

#define ucc_warn(fmt, ...)	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)

#define ucc_info(fmt, ...)	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

#define ucc_dbg(fmt, ...)	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)

#endif
