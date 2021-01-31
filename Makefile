##
 # ScaleVisor Project's Makefile.
 # Copyright (c) 2021, Osmar Cedron, Victor Merckle
##

MODULE_DIR       ?= $(PWD)
ARCH             ?= $(shell uname -r)
KERNELDIR        ?= /lib/modules/${ARCH}
MAIN_FILE		 ?= kthread
INSTALL_MOD_PATH ?= /

obj-m += ${MAIN_FILE}.o


ifndef NTIMES
    # provide a default
    NTIMES=16
endif

ifndef MAXBYTES
    # provide a default
    MAXBYTES=8096
endif

all: modules

modules:
	make -C ${KERNELDIR}/build M=${MODULE_DIR} modules

modules_install:
	make ARCH=${ARCH} INSTALL_MOD_PATH=${INSTALL_MOD_PATH} -C ${KERNELDIR} M=${MODULE_DIR} modules_install
	sudo depmod -a

clean:
	make -C ${KERNELDIR}/build M=${MODULE_DIR} clean

uninstall:
	rm ${MAIN_FILE}.ko
	sudo dmesg
	sudo rmmod ${MAIN_FILE}

run:
	sudo depmod -a
	sudo insmod ${MAIN_FILE}.ko NTIMES=$(NTIMES) MAXBYTES=$(MAXBYTES)

cleanall:
	rm -f *.o *.ko *.mod.c .*.o .*.ko .*.mod.c .*.cmd *~
	rm -f Module.symvers Module.markers modules.order
	rm -rf .tmp_versions
