CC=gcc
LOCAL_CFLAGS=-Wall -Werror

obj-m += kds.o

all: kds.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: kds.ko
	sudo insmod kds.ko int_string='"11 44 22 33 5"' linked_string='"11 44 22 33 5"' bitmap_string='"11 44 22 33 5"' hashmap_string='"11 44 22 33 5"' rbtree_string='"11 44 22 33 5"' radix_string='"11 44 22 33 5"' xarray_string='"11 44 22 33 5"'

uninstall: kds.ko
	sudo rmmod $<

