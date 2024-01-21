obj-m += driver.o
KDIR = /lib/modules/$(shell uname -r)/build
all: clean
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
