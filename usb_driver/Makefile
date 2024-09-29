KERNEL:=/lib/modules/`uname -r`/build
obj-m += usb_dev.o

all:
	make -C ${KERNEL} M=`pwd` modules
clean:
	make -C ${KERNEL} M=`pwd` clean