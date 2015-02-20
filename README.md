# kernel.sshfs

Unlike common sshfs with fuse, this project is totally kernel-origin

# Install Guide:

1. `make ko` under current directory(kernel.sshfs/)
2. `sudo insmod sshfs.ko`
3. `sudo mount -t kernelsshfs -o host=[your remote machine ip] none [mount point]`
4. `make all` under kernel.sshfs/sshfsd
5. run sshfsd with parameter [your remote machine ip]

Example:

    kernel.sshfs> make ko
    kernel.sshfs> sudo insmod sshfs.ko
    kernel.sshfs> sudo mount -t kernelsshfs -o host=222.29.98.5 none mount
    kernel.sshfs> cd sshfsd
    kernel.sshfs/sshfsd> make all
    kernel.sshfs/sshfsd> ./sshfsd 222.29.98.5
    # here to test
    kernel.sshfs/sshfsd> cd ../mount
    kernel.sshfs/mount> sudo vim helloworld

> Please allow publickey authentication to your remote machine first.


# References

1. ramfs: Give me the hints about how to mount a file system without block devices.
2. simplefs: This tiny project enlights me the basic framework of a file system.
3. NO REFERENCE TO sshfs: Yea, even a peak to the source code is banned by myself.
