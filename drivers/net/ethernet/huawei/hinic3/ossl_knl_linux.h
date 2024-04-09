/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2021 Huawei Technologies Co., Ltd */

#ifndef OSSL_KNL_LINUX_H_
#define OSSL_KNL_LINUX_H_

#include <net/ipv6.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/ethtool.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/if_vlan.h>
#include <linux/udp.h>
#include <linux/highmem.h>
#include <linux/list.h>
#include <linux/bitmap.h>
#include <linux/slab.h>

#ifndef NETIF_F_SCTP_CSUM
#define NETIF_F_SCTP_CSUM 0
#endif

#ifndef __GFP_COLD
#define __GFP_COLD 0
#endif

#ifndef __GFP_COMP
#define __GFP_COMP 0
#endif

#undef __always_unused
#define __always_unused __attribute__((__unused__))

#define ossl_get_free_pages __get_free_pages

#ifndef high_16_bits
#define low_16_bits(x) ((x) & 0xFFFF)
#define high_16_bits(x) (((x) & 0xFFFF0000) >> 16)
#endif

#ifndef U8_MAX
#define U8_MAX 0xFF
#endif

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) (((a) << 8) + (b))
#endif
#ifndef AX_RELEASE_VERSION
#define AX_RELEASE_VERSION(a, b) (((a) << 8) + (b))
#endif

#ifndef AX_RELEASE_CODE
#define AX_RELEASE_CODE 0
#endif

#if (AX_RELEASE_CODE && AX_RELEASE_CODE == AX_RELEASE_VERSION(3, 0))
#define RHEL_RELEASE_CODE RHEL_RELEASE_VERSION(5, 0)
#elif (AX_RELEASE_CODE && AX_RELEASE_CODE == AX_RELEASE_VERSION(3, 1))
#define RHEL_RELEASE_CODE RHEL_RELEASE_VERSION(5, 1)
#elif (AX_RELEASE_CODE && AX_RELEASE_CODE == AX_RELEASE_VERSION(3, 2))
#define RHEL_RELEASE_CODE RHEL_RELEASE_VERSION(5, 3)
#endif

#ifndef RHEL_RELEASE_CODE
/* NOTE: RHEL_RELEASE_* introduced in RHEL4.5. */
#define RHEL_RELEASE_CODE 0
#endif

/* RHEL 7 didn't backport the parameter change in
 * create_singlethread_workqueue.
 * If/when RH corrects this we will want to tighten up the version check.
 */
#if (RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 0))
#undef create_singlethread_workqueue
#define create_singlethread_workqueue(name) \
	alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM, name)
#endif

/* Ubuntu Release ABI is the 4th digit of their kernel version. You can find
 * it in /usr/src/linux/$(uname -r)/include/generated/utsrelease.h for new
 * enough versions of Ubuntu. Otherwise you can simply see it in the output of
 * uname as the 4th digit of the kernel. The UTS_UBUNTU_RELEASE_ABI is not in
 * the linux-source package, but in the linux-headers package. It begins to
 * appear in later releases of 14.04 and 14.10.
 *
 * Ex:
 * <Ubuntu 14.04.1>
 * $uname -r
 * 3.13.0-45-generic
 * ABI is 45
 *
 * <Ubuntu 14.10>
 * $uname -r
 * 3.16.0-23-generic
 * ABI is 23.
 */
#ifndef UTS_UBUNTU_RELEASE_ABI
#define UTS_UBUNTU_RELEASE_ABI 0
#define UBUNTU_VERSION_CODE 0
#else

#ifndef __HULK_3_10__
/* Ubuntu does not provide actual release version macro, so we use the kernel
 * version plus the ABI to generate a unique version code specific to Ubuntu.
 * In addition, we mask the lower 8 bits of LINUX_VERSION_CODE in order to
 * ignore differences in sublevel which are not important since we have the
 * ABI value. Otherwise, it becomes impossible to correlate ABI to version for
 * ordering checks.
 */
#define UBUNTU_VERSION_CODE \
	(((~0xFF & LINUX_VERSION_CODE) << 8) + UTS_UBUNTU_RELEASE_ABI)
#endif
#if UTS_UBUNTU_RELEASE_ABI > 255
#error UTS_UBUNTU_RELEASE_ABI is too large...
#endif /* UTS_UBUNTU_RELEASE_ABI > 255 */

#endif

/* Note that the 3rd digit is always zero, and will be ignored. This is
 * because Ubuntu kernels are based on x.y.0-ABI values, and while their linux
 * version codes are 3 digit, this 3rd digit is superseded by the ABI value.
 */
#define UBUNTU_VERSION(a, b, c, d) ((KERNEL_VERSION(a, b, 0) << 8) + (d))

#ifndef DEEPIN_PRODUCT_VERSION
#define DEEPIN_PRODUCT_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#endif

#ifndef DEEPIN_VERSION_CODE
#define DEEPIN_VERSION_CODE 0
#endif

#ifndef ALIGN_DOWN
#ifndef __ALIGN_KERNEL
#define __ALIGN_KERNEL(x, a) __ALIGN_MASK(x, (typeof(x))(a) - 1)
#endif
#define ALIGN_DOWN(x, a) __ALIGN_KERNEL((x) - ((a) - 1), (a))
#endif

#define ETH_TYPE_TRANS_SETS_DEV
#define HAVE_NETDEV_STATS_IN_NETDEV

#if (RHEL_RELEASE_CODE && \
	(RHEL_RELEASE_VERSION(6, 2) <= RHEL_RELEASE_CODE) && \
	(RHEL_RELEASE_VERSION(7, 0) > RHEL_RELEASE_CODE))
#define HAVE_RHEL6_NET_DEVICE_EXTENDED
#endif /* RHEL >= 6.2 && RHEL < 7.0 */
#if (RHEL_RELEASE_CODE && \
	(RHEL_RELEASE_VERSION(6, 6) <= RHEL_RELEASE_CODE) && \
	(RHEL_RELEASE_VERSION(7, 0) > RHEL_RELEASE_CODE))
#define HAVE_RHEL6_NET_DEVICE_OPS_EXT
#define HAVE_NDO_SET_FEATURES
#endif /* RHEL >= 6.6 && RHEL < 7.0 */

#ifndef HAVE_SET_RX_MODE
#define HAVE_SET_RX_MODE
#endif
#define HAVE_INET6_IFADDR_LIST

#define HAVE_NDO_GET_STATS64

#ifndef HAVE_MQPRIO
#define HAVE_MQPRIO
#endif
#ifndef HAVE_SETUP_TC
#define HAVE_SETUP_TC
#endif

#ifndef HAVE_NDO_SET_FEATURES
#define HAVE_NDO_SET_FEATURES
#endif
#define HAVE_IRQ_AFFINITY_NOTIFY

#define HAVE_ETHTOOL_SET_PHYS_ID

#define HAVE_NETDEV_WANTED_FEAUTES

#ifndef HAVE_PCI_DEV_FLAGS_ASSIGNED
#define HAVE_PCI_DEV_FLAGS_ASSIGNED
#define HAVE_VF_SPOOFCHK_CONFIGURE
#endif
#ifndef HAVE_SKB_L4_RXHASH
#define HAVE_SKB_L4_RXHASH
#endif

#define HAVE_ETHTOOL_GRXFHINDIR_SIZE
#define HAVE_INT_NDO_VLAN_RX_ADD_VID
#ifdef ETHTOOL_SRXNTUPLE
#undef ETHTOOL_SRXNTUPLE
#endif


#define _kc_kmap_atomic(page) kmap_atomic(page)
#define _kc_kunmap_atomic(addr) kunmap_atomic(addr)

#include <linux/of_net.h>
#define HAVE_FDB_OPS
#define HAVE_ETHTOOL_GET_TS_INFO

#define HAVE_NAPI_GRO_FLUSH_OLD

#ifndef HAVE_SRIOV_CONFIGURE
#define HAVE_SRIOV_CONFIGURE
#endif


#define HAVE_ENCAP_TSO_OFFLOAD
#define HAVE_SKB_INNER_NETWORK_HEADER
#if (RHEL_RELEASE_CODE && \
	(RHEL_RELEASE_VERSION(7, 0) <= RHEL_RELEASE_CODE) && \
	(RHEL_RELEASE_VERSION(8, 0) > RHEL_RELEASE_CODE))
#define HAVE_RHEL7_PCI_DRIVER_RH
#if (RHEL_RELEASE_VERSION(7, 2) <= RHEL_RELEASE_CODE)
#define HAVE_RHEL7_PCI_RESET_NOTIFY
#endif /* RHEL >= 7.2 */
#if (RHEL_RELEASE_VERSION(7, 3) <= RHEL_RELEASE_CODE)
#define HAVE_GENEVE_RX_OFFLOAD
#if !defined(HAVE_UDP_ENC_TUNNEL) && IS_ENABLED(CONFIG_GENEVE)
#define HAVE_UDP_ENC_TUNNEL
#endif
#ifdef ETHTOOL_GLINKSETTINGS
/* pay attention pangea platform when use this micro */
#define HAVE_ETHTOOL_25G_BITS
#endif /* ETHTOOL_GLINKSETTINGS */
#endif /* RHEL >= 7.3 */

/* new hooks added to net_device_ops_extended in RHEL7.4 */
#if (RHEL_RELEASE_VERSION(7, 4) <= RHEL_RELEASE_CODE)
#define HAVE_RHEL7_NETDEV_OPS_EXT_NDO_UDP_TUNNEL
#define HAVE_UDP_ENC_RX_OFFLOAD
#endif /* RHEL >= 7.4 */

#if (RHEL_RELEASE_VERSION(7, 5) <= RHEL_RELEASE_CODE)
#define HAVE_NDO_SETUP_TC_REMOVE_TC_TO_NETDEV
#endif /* RHEL > 7.5 */

#endif /* RHEL >= 7.0 && RHEL < 8.0 */

/* ************************************************************************ */
#define HAVE_NDO_SET_VF_LINK_STATE
#define HAVE_SKB_INNER_PROTOCOL
#define HAVE_MPLS_FEATURES

#define HAVE_VXLAN_CHECKS
#if (UBUNTU_VERSION_CODE && \
	UBUNTU_VERSION(3, 13, 0, 24) <= UBUNTU_VERSION_CODE)
#define HAVE_NDO_SELECT_QUEUE_ACCEL_FALLBACK
#else
#define HAVE_NDO_SELECT_QUEUE_ACCEL
#endif
#define HAVE_NET_GET_RANDOM_ONCE
#define HAVE_HWMON_DEVICE_REGISTER_WITH_GROUPS



#define HAVE_NDO_SELECT_QUEUE_ACCEL_FALLBACK



#define HAVE_NDO_SET_VF_MIN_MAX_TX_RATE
#define HAVE_VLAN_FIND_DEV_DEEP_RCU



#define HAVE_SKBUFF_CSUM_LEVEL
#define HAVE_MULTI_VLAN_OFFLOAD_EN
#define HAVE_ETH_GET_HEADLEN_FUNC


#define HAVE_RXFH_HASHFUNC

#define HAVE_NDO_SET_VF_TRUST


#include <net/devlink.h>

#define HAVE_IO_MAP_WC_SIZE
#define HAVE_NETDEVICE_MIN_MAX_MTU
#define HAVE_VOID_NDO_GET_STATS64
#define HAVE_VM_OPS_FAULT_NO_VMA

/* ************************************************************************ */
#define HAVE_HWTSTAMP_FILTER_NTP_ALL
#define HAVE_NDO_SETUP_TC_CHAIN_INDEX
#define HAVE_PCI_ERROR_HANDLER_RESET_PREPARE
#define HAVE_PTP_CLOCK_DO_AUX_WORK

#define HAVE_NDO_SETUP_TC_REMOVE_TC_TO_NETDEV

#define HAVE_XDP_SUPPORT

#define HAVE_NDO_BPF_NETDEV_BPF
#define HAVE_TIMER_SETUP
#define HAVE_XDP_DATA_META

#define HAVE_MACRO_VM_FAULT_T


#if RHEL_RELEASE_CODE && RHEL_RELEASE_VERSION(8, 2) <= RHEL_RELEASE_CODE
#define ETH_GET_HEADLEN_NEED_DEV
#endif


#define HAVE_NDO_SELECT_QUEUE_SB_DEV

#define dev_open(x) dev_open(x, NULL)
#define HAVE_NEW_ETHTOOL_LINK_SETTINGS_ONLY

#ifndef get_ds
#define get_ds()	(KERNEL_DS)

#ifndef dma_zalloc_coherent
#define dma_zalloc_coherent(d, s, h, f) _hinic3_dma_zalloc_coherent(d, s, h, f)
static inline void *_hinic3_dma_zalloc_coherent(struct device *dev,
						size_t size, dma_addr_t *dma_handle,
						gfp_t gfp)
{
	/* Above kernel 5.0, fixed up all remaining architectures
	 * to zero the memory in dma_alloc_coherent, and made
	 * dma_zalloc_coherent a no-op wrapper around dma_alloc_coherent,
	 * which fixes all of the above issues.
	 */
	return dma_alloc_coherent(dev, size, dma_handle, gfp);
}
#endif

#ifndef DT_KNL_EMU
struct timeval {
	__kernel_old_time_t     tv_sec;         /* seconds */
	__kernel_suseconds_t    tv_usec;        /* microseconds */
};
#endif

#ifndef do_gettimeofday
#define do_gettimeofday(time) _kc_do_gettimeofday(time)
static inline void _kc_do_gettimeofday(struct timeval *tv)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}
#endif

#endif /* 5.0.0 */

#define HAVE_NDO_SELECT_QUEUE_SB_DEV_ONLY
#define ETH_GET_HEADLEN_NEED_DEV
#define HAVE_GENL_OPS_FIELD_VALIDATE


#ifndef FIELD_SIZEOF
#define FIELD_SIZEOF(t, f) (sizeof(((t *)0)->f))
#endif


#define HAVE_DEVLINK_FLASH_UPDATE_PARAMS

#ifndef rtc_time_to_tm
#define rtc_time_to_tm rtc_time64_to_tm
#endif
#define HAVE_NDO_TX_TIMEOUT_TXQ
#define HAVE_PROC_OPS

#define SUPPORTED_COALESCE_PARAMS

#ifndef pci_cleanup_aer_uncorrect_error_status
#define pci_cleanup_aer_uncorrect_error_status pci_aer_clear_nonfatal_status
#endif

#define HAVE_XDP_FRAME_SZ

#ifndef HAVE_ETHTOOL_COALESCE_EXTACK
#define HAVE_ETHTOOL_COALESCE_EXTACK
#endif

#ifndef HAVE_ETHTOOL_RINGPARAM_EXTACK
#define HAVE_ETHTOOL_RINGPARAM_EXTACK
#endif

/* This defines the direction arg to the DMA mapping routines. */
#define PCI_DMA_BIDIRECTIONAL	0
#define PCI_DMA_TODEVICE	1
#define PCI_DMA_FROMDEVICE	2
#define PCI_DMA_NONE		3

static inline dma_addr_t
pci_map_single(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
	return dma_map_single(hwdev == NULL ? NULL : &hwdev->dev, ptr, size, (enum dma_data_direction)direction);
}

static inline void
pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
		 size_t size, int direction)
{
	dma_unmap_single(hwdev == NULL ? NULL : &hwdev->dev, dma_addr, size, (enum dma_data_direction)direction);
}

static inline int
pci_dma_mapping_error(struct pci_dev *hwdev, dma_addr_t dma_addr)
{
	return dma_mapping_error(&hwdev->dev, dma_addr);
}

static inline void *
pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
		     dma_addr_t *dma_handle)
{
	return dma_alloc_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, dma_handle, GFP_ATOMIC);
}

static inline void
pci_free_consistent(struct pci_dev *hwdev, size_t size,
		    void *vaddr, dma_addr_t dma_handle)
{
	dma_free_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, vaddr, dma_handle);
}

#define HAVE_ENCAPSULATION_TSO
#define HAVE_ENCAPSULATION_CSUM

#ifndef eth_zero_addr
static inline void hinic3_eth_zero_addr(u8 *addr)
{
	memset(addr, 0x00, ETH_ALEN);
}

#define eth_zero_addr(_addr) hinic3_eth_zero_addr(_addr)
#endif

#ifndef netdev_hw_addr_list_for_each
#define netdev_hw_addr_list_for_each(ha, l) \
	list_for_each_entry(ha, &(l)->list, list)
#endif

#define spin_lock_deinit(lock)

struct file *file_creat(const char *file_name);

struct file *file_open(const char *file_name);

void file_close(struct file *file_handle);

u32 get_file_size(struct file *file_handle);

void set_file_position(struct file *file_handle, u32 position);

int file_read(struct file *file_handle, char *log_buffer, u32 rd_length,
	      u32 *file_pos);

u32 file_write(struct file *file_handle, const char *log_buffer, u32 wr_length);

struct sdk_thread_info {
	struct task_struct *thread_obj;
	char *name;
	void (*thread_fn)(void *x);
	void *thread_event;
	void *data;
};

int creat_thread(struct sdk_thread_info *thread_info);

void stop_thread(struct sdk_thread_info *thread_info);

#define destroy_work(work)
void utctime_to_localtime(u64 utctime, u64 *localtime);
#ifndef HAVE_TIMER_SETUP
void initialize_timer(const void *adapter_hdl, struct timer_list *timer);
#endif
void add_to_timer(struct timer_list *timer, u64 period);
void stop_timer(struct timer_list *timer);
void delete_timer(struct timer_list *timer);
u64 ossl_get_real_time(void);

#define nicif_err(priv, type, dev, fmt, args...) \
	netif_level(err, priv, type, dev, "[NIC]" fmt, ##args)
#define nicif_warn(priv, type, dev, fmt, args...) \
	netif_level(warn, priv, type, dev, "[NIC]" fmt, ##args)
#define nicif_notice(priv, type, dev, fmt, args...) \
	netif_level(notice, priv, type, dev, "[NIC]" fmt, ##args)
#define nicif_info(priv, type, dev, fmt, args...) \
	netif_level(info, priv, type, dev, "[NIC]" fmt, ##args)
#define nicif_dbg(priv, type, dev, fmt, args...) \
	netif_level(dbg, priv, type, dev, "[NIC]" fmt, ##args)

#define destroy_completion(completion)
#define sema_deinit(lock)
#define mutex_deinit(lock)
#define rwlock_deinit(lock)

#define tasklet_state(tasklet) ((tasklet)->state)

#endif
/* ************************************************************************ */
