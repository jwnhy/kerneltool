KERNEL_PATH ?= /lib/modules/$(shell uname -r)/build

obj-m := koohm.o
koohm-objs := kooh.o kooh_test.o
EXTRA_CFLAGS += -DDEBUG


all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
