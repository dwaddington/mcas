
KERNEL_VER = `uname -r`
BUILD = `date +%Y%m%d.%k%m`

ccflags-y := -g -Wall

obj-m += mcasmod.o

mcasmod-objs := mcas.o page_walk.o

all: mcasmod.ko

mcasmod.ko:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean

install:
	echo Installing mcasmod.ko to $(PREFIX)/bin ...
	install -d $(PREFIX)/bin
	install -m 444 mcasmod.ko $(PREFIX)/bin

.PHONY: install clean
