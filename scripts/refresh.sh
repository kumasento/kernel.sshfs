
#!/bin/sh

umount mount
rmmod sshfs
insmod sshfs.ko
mount -t kernelsshfs -o host=222.29.98.5 none mount
