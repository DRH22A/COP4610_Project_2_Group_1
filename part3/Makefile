KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

obj-m := elevator.o
elevator-objs := src/elevator.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	sudo insmod elevator.ko

unload:
	sudo rmmod elevator

reload: unload load

.PHONY: all clean load unload reload