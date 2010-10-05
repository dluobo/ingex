ifneq ($(KERNELRELEASE),)
# kbuild part of makefile

obj-m := dvsdriver_lucy.o
obj-m += dvsdriver_sdio.o

dvsdriver_lucy-y := ../../development/src_lucy/driver/linux.o dvsdriver_lucy_pre.o
dvsdriver_sdio-y := ../../development/src_sdio/driver/linux.o dvsdriver_sdio_pre.o

$(src)/dvsdriver_lucy_pre.o_shipped: $(src)/dvsdriver_lucy.pre
	ln -s $(src)/dvsdriver_lucy.pre $(src)/dvsdriver_lucy_pre.o_shipped

$(src)/dvsdriver_sdio_pre.o_shipped: $(src)/dvsdriver_sdio.pre
	ln -s $(src)/dvsdriver_sdio.pre $(src)/dvsdriver_sdio_pre.o_shipped

else
# normal makefile

KERNELDIR := /lib/modules/`uname -r`/build
all::
	$(MAKE) -C $(KERNELDIR) M=`pwd`

endif

