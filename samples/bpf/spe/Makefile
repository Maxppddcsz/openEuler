# SPDX-License-Identifier: GPL-2.0

include Makefile.arch

INSTALL ?= install
CLANG ?= clang
CC ?= gcc

BPFTOOL ?= bpftool
KERNEL_DIR ?= ../../../

MKFLAGS = -I$(KERNEL_DIR)/tools/lib -I$(KERNEL_DIR)/tools/include/uapi/ \
	-D__BPF_TRACING__ -D__TARGET_ARCH_${SRCARCH}
LDLIBBPF = -L$(KERNEL_DIR)/tools/lib/bpf/ -l:libbpf.a

all:
	$(CLANG) -O2 -g -Wall -target bpf -I. ${MKFLAGS} -c spe-record.bpf.c -o spe-record.bpf.o
	$(BPFTOOL) gen skeleton spe-record.bpf.o > spe-record.skel.h
	$(CC) -O2 -g -Wall ${MKFLAGS} spe-record.user.c -o spe-record ${LDLIBBPF} -lelf -lz --static

clean:
	rm -f spe-record
	rm -f vmlinux.h
	rm -f *.o
	rm -f *.skel.h
