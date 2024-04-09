EXPORT_SYMBOL := true

GLOBAL_VERSION="16.13.8.1"
OFED_VERSION := OFED_MLNX_5_8

KBUILD_EXTRA_SYMBOLS += $(srctree)/drivers/net/ethernet/huawei/hinic3/Module.symvers

EXTRA_CFLAGS += -DHW_CONVERT_ENDIAN
EXTRA_CFLAGS += -DROCE_SERVICE
EXTRA_CFLAGS += -D__ROCE_DFX__
EXTRA_CFLAGS += -DROCE_CC_EN
EXTRA_CFLAGS += -Werror -Wall

EXTRA_CFLAGS += -Werror -Wno-implicit-fallthrough

EXTRA_CFLAGS += -fstack-protector-all

ORIGN_LINUXINCLUDE := $(LINUXINCLUDE)

THIRDPARTY_INC = /usr/src/ofa_kernel/default/
EXTRA_CFLAGS += -D$(OFED_VERSION)
LINUXINCLUDE := -I$(THIRDPARTY_INC)/include/ -I$(THIRDPARTY_INC)/include/uapi $(ORIGN_LINUXINCLUDE)

EXTRA_CFLAGS += -DROCE_COMPUTE
EXTRA_CFLAGS += -DROCE_BONDING_EN
EXTRA_CFLAGS += -DROCE_STANDARD
EXTRA_CFLAGS += -DOFED_MLNX_5_8

# Set CFLAGS from default file directories
EXTRA_CFLAGS += -I$(srctree)/include/linux
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/host/hmm
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/bond
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/cq
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/dfx
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/extension
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/include
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/include/nic
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/include/rdma
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/include/mag
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/mr
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/qp
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/rdma
EXTRA_CFLAGS += -I$(srctree)/drivers/infiniband/hw/hiroce3/srq
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3/hw
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3/bond
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3/include
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3/include/cfg_mgmt
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3/include/mpu
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3/include/bond
EXTRA_CFLAGS += -I$(srctree)/drivers/net/ethernet/huawei/hinic3/include/cqm

export KLIB_BUILD
export KERN_VERSION
export MAKE
export KBUILD_EXTRA_SYMBOLS

$(info ============== INFO ==============)
$(info Building kernel modules)
$(info Driver Version: $(VERSION))
$(info Kernel Version: $(KERN_VERSION))
$(info os type: $(HI1823_OS_TYPE))
$(info Kernel Sources: $(KSRC))
$(info Thirdparty Source: $(THIRDPARTY_INC))
$(info linux include: $(LINUXINCLUDE))
$(info Thirdparty Symbols: $(KBUILD_EXTRA_SYMBOLS))
$(info build_dir: $(HI1823_BUILD_DIR))
$(info ==================================)
$(info linux version $(LINUX_VERSION_CODE))
$(info $(EXTRA_CFLAGS))
$(info ==================================)

obj-$(CONFIG_HIROCE3) += hiroce3.o

hiroce3-y := cq/roce_cq_common.o \
            cq/roce_cq_cqe.o \
            cq/roce_cq_create.o \
            cq/roce_cq_ctrl.o \
            cq/roce_cq_destroy.o \
            extension/roce_event_extension.o \
            extension/roce_main_extension.o \
            extension/roce_mr_extension.o \
            extension/roce_netdev_extension.o \
            extension/roce_qp_extension.o \
            extension/roce_qp_post_send_extension.o \
            extension/roce_srq_extension.o \
            extension/roce_cdev_extension.o \
            roce_db.o \
            roce_main.o \
            roce_netlink.o \
            roce_event.o \
            roce_netdev.o \
            roce_mix.o \
            mr/roce_mr.o \
            roce_pd.o \
            qp/roce_qp_create.o \
            qp/roce_qp_destroy.o \
            qp/roce_qp_modify.o \
            qp/roce_qp_post_recv.o \
            qp/roce_qp_post_send.o \
            qp/roce_qp_query.o \
            roce_xrc.o \
            dfx/roce_dfx.o \
            dfx/roce_dfx_query.o \
            dfx/roce_dfx_cap.o \
            roce_cdev.o \
            roce_sysfs.o \
            roce_cmd.o \
            roce_cqm_cmd.o \
            bond/roce_bond_common.o \
            rdma/rdma_bitmap.o \
            rdma/rdma_comp.o \
            rdma/rdma_comp_res.o \
            rdma/rdma_comp_gid.o \
            rdma/rdma_comp_init.o \
            rdma/rdma_comp_pd.o \
            rdma/rdma_comp_mw_mr.o \
            srq/roce_srq_comm.o \
            srq/roce_srq_create.o \
            srq/roce_srq_ctrl.o \
            host/hmm/hmm_buddy.o \
            host/hmm/hmm_comp.o \
            host/hmm/hmm_comp_init.o \
            host/hmm/hmm_comp_mtt.o \
            host/hmm/hmm_comp_mw_mr.o \
            host/hmm/hmm_comp_res.o \
            host/hmm/hmm_em.o \
            host/hmm/hmm_mr.o \
            host/hmm/hmm_umem.o

SYS_TIME=$(shell date +%Y-%m-%d_%H:%M:%S)
ccflags-y += -D __TIME_STR__=\"$(SYS_TIME)\"
ccflags-y += -DGLOBAL_VERSION_STR=\"$(GLOBAL_VERSION)\"

$(warning cflags, $(ccflags-y))
V ?= 0

ccflags-y += -DHW_CONVERT_ENDIAN

ccflags-y += -D__LINUX__

EXTRA_CFLAGS += -DSECUREC_EXPORT_KERNEL_SYMBOL=0

KERNEL_VER := $(shell uname -r 2>null)

KERNEL_DIR := /lib/modules/$(KERNEL_VER)/build

default:
	$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) -W modules

clean:
	rm -rf *.o *.ko *.order .*.cmd *.mod.* .H* .tm* .tmp_versions Module.symvers *.ko.unsigned null
