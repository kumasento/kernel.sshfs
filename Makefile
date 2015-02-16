
obj-m := sshfs.o
sshfs-objs := sshfs_inode.o
ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
