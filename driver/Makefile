# SPDX-License-Identifier: BSD-3-Clause

DRIVER=keystone-driver.ko

PWD := $(shell pwd)
LINUXSRC ?= $(PWD)/../linux
CROSS_COMPILE ?= riscv64-unknown-linux-gnu-

keystone-driver-y := \
	keystone.o \
	keystone-page.o \
	keystone-ioctl.o \
	keystone-enclave.o \
	keystone-sbi.o
obj-m += keystone-driver.o

default:
	$(MAKE) -C $(LINUXSRC) ARCH=riscv CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

install:
	$(MAKE) -C $(LINUXSRC) ARCH=riscv CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules_install

clean:
	rm -rvf *.o *.o.d *.ko *.order *.symvers *.mod.c .tmp_versions .*.cmd *.mod
