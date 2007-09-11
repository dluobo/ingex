INSTRUCTIONS:

1) Check patch file and modified sources match the kernel sources


2) copy modified sources to kernel sources


3) compile the kernel usbhid module
> cd /usr/src/linux
> make mrproper
> make cloneconfig
> make scripts
> make prepare

> cd /usr/src/linux/drivers/usb/input
> make -C /usr/src/linux M=`pwd`


4) make module directory
> mkdir /lib/modules/`uname -r`/contour


5) copy usbhid.ko
> cp /usr/src/linux/drivers/usb/input/usbhid.ko /lib/modules/`uname -r`/contour


6) copy setup scipt
> cp qc_player_setup /etc/init.d/


7) install setup script
> /sbin/chkconfig --add qc_player_setup
> /sbin/chkconfig qc_player_setup 35
> /sbin/chkconfig --terse qc_player_setup


8) check
> /etc/init.d/qc_player_setup start

