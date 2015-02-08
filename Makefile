
obj-m := sshfs.o
sshfs-objs := inode.o

ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
