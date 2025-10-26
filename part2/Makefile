KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

obj-m := my_timer.o
my_timer-objs := src/my_timer.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	sudo insmod my_timer.ko

unload:
	sudo rmmod my_timer

reload: unload load
